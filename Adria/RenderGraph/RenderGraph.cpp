#include <algorithm>
#include <optional>
#include <d3d12.h>
#include "pix3.h"
#include "RenderGraph.h"
#include "../Graphics/GfxCommandList.h"
#include "../Graphics/GfxRenderPass.h"
#include "../Graphics/GPUProfiler.h"
#include "../Tasks/TaskSystem.h"
#include "../Utilities/StringUtil.h"


#if GPU_MULTITHREADED
#define RG_MULTITHREADED 1
#else 
#define RG_MULTITHREADED 0
#endif

namespace adria
{
	RGTextureId RenderGraph::DeclareTexture(RGResourceName name, RGTextureDesc const& desc)
	{
		ADRIA_ASSERT(texture_name_id_map.find(name) == texture_name_id_map.end() && "Texture with that name has already been declared");
		GfxTextureDesc tex_desc{}; FillTextureDesc(desc, tex_desc);
		textures.emplace_back(new RGTexture(textures.size(), tex_desc, name));
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
		return RGTextureId(textures.size() - 1);
	}

	RGBufferId RenderGraph::DeclareBuffer(RGResourceName name, RGBufferDesc const& desc)
	{
		ADRIA_ASSERT(buffer_name_id_map.find(name) == buffer_name_id_map.end() && "Buffer with that name has already been declared");
		GfxBufferDesc buf_desc{}; FillBufferDesc(desc, buf_desc);
		buffers.emplace_back(new RGBuffer(buffers.size(), buf_desc, name));
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
		return RGBufferId(buffers.size() - 1);
	}

	bool RenderGraph::IsTextureDeclared(RGResourceName name)
	{
		return texture_name_id_map.find(name) != texture_name_id_map.end();
	}

	bool RenderGraph::IsBufferDeclared(RGResourceName name)
	{
		return buffer_name_id_map.find(name) != buffer_name_id_map.end();
	}

	void RenderGraph::ImportTexture(RGResourceName name, GfxTexture* texture)
	{
		textures.emplace_back(new RGTexture(textures.size(), texture, name));
		textures.back()->SetName();
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
	}

	void RenderGraph::ImportBuffer(RGResourceName name, GfxBuffer* buffer)
	{
		buffers.emplace_back(new RGBuffer(buffers.size(), buffer, name));
		buffers.back()->SetName();
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
	}

	bool RenderGraph::IsValidTextureHandle(RGTextureId handle) const
	{
		return handle.IsValid() && handle.id < textures.size();
	}

	bool RenderGraph::IsValidBufferHandle(RGBufferId handle) const
	{
		return handle.IsValid() && handle.id < buffers.size();
	}

	RenderGraph::~RenderGraph()
	{
		for (auto& [tex_id, view_vector] : texture_view_map)
		{
			for (auto [view, type] : view_vector)
			{
				switch (type)
				{
				case RGDescriptorType::RenderTarget:
					gfx->FreeOfflineDescriptor(view, GfxDescriptorHeapType::RTV);
					continue;
				case RGDescriptorType::DepthStencil:
					gfx->FreeOfflineDescriptor(view, GfxDescriptorHeapType::DSV);
					continue;
				default:
					gfx->FreeOfflineDescriptor(view, GfxDescriptorHeapType::CBV_SRV_UAV);
				}
			}
		}

		for (auto& [buf_id, view_vector] : buffer_view_map)
		{
			for (auto [view, type] : view_vector) gfx->FreeOfflineDescriptor(view, GfxDescriptorHeapType::CBV_SRV_UAV);
		}
	}

	void RenderGraph::Build()
	{
		BuildAdjacencyLists();
		TopologicalSort();
		BuildDependencyLevels();
		CullPasses();
		CalculateResourcesLifetime();
	}

	void RenderGraph::Execute()
	{
#if RG_MULTITHREADED
		Execute_Multithreaded();
#else
		Execute_Singlethreaded();
#endif
	}

	void RenderGraph::Execute_Singlethreaded()
	{
		pool.Tick();

		for (auto& dependency_level : dependency_levels) dependency_level.Setup();

		GfxCommandList* cmd_list = gfx->GetCommandList(GfxCommandListType::Graphics);
		for (size_t i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto tex_id : dependency_level.texture_creates)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				rg_texture->resource = pool.AllocateTexture(rg_texture->desc);
				CreateTextureViews(tex_id);
				rg_texture->SetName();
			}
			for (auto buf_id : dependency_level.buffer_creates)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				rg_buffer->resource = pool.AllocateBuffer(rg_buffer->desc);
				CreateBufferViews(buf_id);
				rg_buffer->SetName();
			}

			{
				for (auto const& [tex_id, state] : dependency_level.texture_state_map)
				{
					RGTexture* rg_texture = GetRGTexture(tex_id);
					GfxTexture* texture = rg_texture->resource;
					if (dependency_level.texture_creates.contains(tex_id))
					{
						if (!HasAllFlags(texture->GetDesc().initial_state, state))
						{
							ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
							cmd_list->TransitionBarrier(*texture, texture->GetDesc().initial_state, state);
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.texture_state_map.contains(tex_id))
						{
							ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
							GfxResourceState prev_state = prev_dependency_level.texture_state_map[tex_id];
							if (prev_state != state) cmd_list->TransitionBarrier(*texture, prev_state, state);
							found = true;
							break;
						}
					}
					if (!found && rg_texture->imported)
					{
						ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
						GfxResourceState prev_state = rg_texture->desc.initial_state;
						if (prev_state != state) cmd_list->TransitionBarrier(*texture, prev_state, state);
					}
				}
				for (auto const& [buf_id, state] : dependency_level.buffer_state_map)
				{
					RGBuffer* rg_buffer = GetRGBuffer(buf_id);
					GfxBuffer* buffer = rg_buffer->resource;
					if (dependency_level.buffer_creates.contains(buf_id))
					{
						if (state != GfxResourceState::Common) //check if there is an implicit transition, maybe this can be avoided
						{
							ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
							cmd_list->TransitionBarrier(*buffer, GfxResourceState::Common, state);
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.buffer_state_map.contains(buf_id))
						{
							ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
							GfxResourceState prev_state = prev_dependency_level.buffer_state_map[buf_id];
							if (prev_state != state) cmd_list->TransitionBarrier(*buffer, prev_state, state);
							found = true;
							break;
						}
					}
					if (!found && rg_buffer->imported)
					{
						ADRIA_ASSERT(IsValidState(state) && "Invalid State Combination!");
						if (GfxResourceState::Common != state)cmd_list->TransitionBarrier(*buffer, GfxResourceState::Common, state);
					}
				}
			}
			cmd_list->FlushBarriers();
			dependency_level.Execute(gfx, cmd_list);

			for (RGTextureId tex_id : dependency_level.texture_destroys)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				GfxTexture* texture = rg_texture->resource;
				GfxResourceState initial_state = texture->GetDesc().initial_state;
				ADRIA_ASSERT(dependency_level.texture_state_map.contains(tex_id));
				GfxResourceState state = dependency_level.texture_state_map[tex_id];
				if (initial_state != state) cmd_list->TransitionBarrier(*texture, state, initial_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
			for (RGBufferId buf_id : dependency_level.buffer_destroys)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				GfxBuffer* buffer = rg_buffer->resource;
				ADRIA_ASSERT(dependency_level.buffer_state_map.contains(buf_id));
				GfxResourceState state = dependency_level.buffer_state_map[buf_id];
				if(state != GfxResourceState::Common) cmd_list->TransitionBarrier(*buffer, state, GfxResourceState::Common);
				if (!rg_buffer->imported) pool.ReleaseBuffer(rg_buffer->resource);
			}
			cmd_list->FlushBarriers();
		}
	}

	void RenderGraph::Execute_Multithreaded()
	{
		ADRIA_ASSERT_MSG(false, "Not implemented!");
	}

	void RenderGraph::BuildAdjacencyLists()
	{
		adjacency_lists.resize(passes.size());
		for (size_t i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			std::vector<uint64>& pass_adjacency_list = adjacency_lists[i];
			for (size_t j = i + 1; j < passes.size(); ++j)
			{
				auto& other_pass = passes[j];
				bool depends = false;
				for (auto other_node_read : other_pass->texture_reads)
				{
					if (pass->texture_writes.find(other_node_read) != pass->texture_writes.end())
					{
						pass_adjacency_list.push_back(j);
						depends = true;
						break;
					}
				}
				if (depends) continue;

				for (auto other_node_read : other_pass->buffer_reads)
				{
					if (pass->buffer_writes.find(other_node_read) != pass->buffer_writes.end())
					{
						pass_adjacency_list.push_back(j);
						break;
					}
				}
			}
		}
	}

	void RenderGraph::TopologicalSort()
	{
		std::stack<size_t> stack{};
		std::vector<bool>  visited(passes.size(), false);
		for (size_t i = 0; i < passes.size(); i++)
		{
			if (visited[i] == false) DepthFirstSearch(i, visited, stack);
		}

		while (!stack.empty())
		{
			size_t i = stack.top();
			topologically_sorted_passes.push_back(i);
			stack.pop();
		}
	}

	void RenderGraph::BuildDependencyLevels()
	{
		std::vector<size_t> distances(topologically_sorted_passes.size(), 0);
		for (size_t u = 0; u < topologically_sorted_passes.size(); ++u)
		{
			size_t i = topologically_sorted_passes[u];
			for (auto v : adjacency_lists[i])
			{
				if (distances[v] < distances[i] + 1) distances[v] = distances[i] + 1;
			}
		}

		dependency_levels.resize(*std::max_element(std::begin(distances), std::end(distances)) + 1, DependencyLevel(*this));
		for (size_t i = 0; i < passes.size(); ++i)
		{
			size_t level = distances[i];
			dependency_levels[level].AddPass(passes[i].get());
		}
	}

	void RenderGraph::CullPasses()
	{
		for (auto& pass : passes)
		{
			pass->ref_count = pass->texture_writes.size() + pass->buffer_writes.size();
			for (auto id : pass->texture_reads)
			{
				auto* consumed = GetRGTexture(id);
				++consumed->ref_count;
			}
			for (auto id : pass->buffer_reads)
			{
				auto* consumed = GetRGBuffer(id);
				++consumed->ref_count;
			}

			for (auto id : pass->texture_writes)
			{
				auto* written = GetRGTexture(id);
				written->writer = pass.get();
			}
			for (auto id : pass->buffer_writes)
			{
				auto* written = GetRGBuffer(id);
				written->writer = pass.get();
			}
		}

		std::stack<RenderGraphResource*> zero_ref_resources;
		for (auto& texture : textures) if (texture->ref_count == 0) zero_ref_resources.push(texture.get());
		for (auto& buffer : buffers)   if (buffer->ref_count == 0) zero_ref_resources.push(buffer.get());

		while (!zero_ref_resources.empty())
		{
			RenderGraphResource* unreferenced_resource = zero_ref_resources.top();
			zero_ref_resources.pop();
			auto* writer = unreferenced_resource->writer;
			if (writer == nullptr || !writer->CanBeCulled()) continue;

			if (--writer->ref_count == 0)
			{
				for (auto id : writer->texture_reads)
				{
					auto* texture = GetRGTexture(id);
					if (--texture->ref_count == 0) zero_ref_resources.push(texture);
				}
				for (auto id : writer->buffer_reads)
				{
					auto* buffer = GetRGBuffer(id);
					if (--buffer->ref_count == 0) zero_ref_resources.push(buffer);
				}
			}
		}
	}

	void RenderGraph::CalculateResourcesLifetime()
	{
		for (size_t i = 0; i < topologically_sorted_passes.size(); ++i)
		{
			auto& pass = passes[topologically_sorted_passes[i]];
			if (pass->IsCulled()) continue;
			for (auto id : pass->texture_writes)
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
			for (auto id : pass->buffer_writes)
			{
				RGBuffer* rg_buffer = GetRGBuffer(id);
				rg_buffer->last_used_by = pass.get();
			}
				
			for (auto id : pass->texture_reads)
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
			for (auto id : pass->buffer_reads)
			{
				RGBuffer* rg_buffer = GetRGBuffer(id);
				rg_buffer->last_used_by = pass.get();
			}
		}
		for (size_t i = 0; i < textures.size(); ++i)
		{
			if (textures[i]->last_used_by != nullptr) textures[i]->last_used_by->texture_destroys.insert(RGTextureId(i));
			if (textures[i]->imported) CreateTextureViews(RGTextureId(i));
		}
		for (size_t i = 0; i < buffers.size(); ++i)
		{
			if (buffers[i]->last_used_by != nullptr) buffers[i]->last_used_by->buffer_destroys.insert(RGBufferId(i));
			if (buffers[i]->imported) CreateBufferViews(RGBufferId(i));
		}
	}

	void RenderGraph::DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack)
	{
		visited[i] = true;
		for (auto j : adjacency_lists[i])
		{
			if (!visited[j]) DepthFirstSearch(j, visited, stack);
		}
		stack.push(i);
	}

	RGTexture* RenderGraph::GetRGTexture(RGTextureId handle) const
	{
		return textures[handle.id].get();
	}

	RGBuffer* RenderGraph::GetRGBuffer(RGBufferId handle) const
	{
		return buffers[handle.id].get();
	}

	GfxTexture* RenderGraph::GetTexture(RGTextureId res_id) const
	{
		return GetRGTexture(res_id)->resource;
	}

	GfxBuffer* RenderGraph::GetBuffer(RGBufferId res_id) const
	{
		return GetRGBuffer(res_id)->resource;
	}

	void RenderGraph::CreateTextureViews(RGTextureId res_id)
	{
		auto const& view_descs = texture_view_desc_map[res_id];
		for (auto const& [view_desc, type] : view_descs)
		{
			GfxTexture* texture = GetTexture(res_id);
			GfxDescriptor view;
			switch (type)
			{
			case RGDescriptorType::RenderTarget:
				view = gfx->CreateTextureRTV(texture, &view_desc);
				break;
			case RGDescriptorType::DepthStencil:
				view = gfx->CreateTextureDSV(texture, &view_desc);
				break;
			case RGDescriptorType::ReadOnly:
				view = gfx->CreateTextureSRV(texture, &view_desc);
				break;
			case RGDescriptorType::ReadWrite:
				view = gfx->CreateTextureUAV(texture, &view_desc);
				break;
			default:
				ADRIA_ASSERT(false && "invalid resource view type for texture");
			}
			texture_view_map[res_id].emplace_back(view, type);
		}
	}

	void RenderGraph::CreateBufferViews(RGBufferId res_id)
	{
		auto const& view_descs = buffer_view_desc_map[res_id];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [view_desc, type] = view_descs[i];
			GfxBuffer* buffer = GetBuffer(res_id);
			GfxDescriptor view;
			switch (type)
			{
			case RGDescriptorType::ReadOnly:
			{
				view = gfx->CreateBufferSRV(buffer, &view_desc);
				break;
			}
			case RGDescriptorType::ReadWrite:
			{
				RGBufferReadWriteId rw_id(i, res_id);
				if (buffer_uav_counter_map.contains(rw_id))
				{
					GfxBuffer* counter_buffer = GetBuffer(buffer_uav_counter_map[rw_id]);
					view = gfx->CreateBufferUAV(buffer, counter_buffer, &view_desc);
				}
				else view = gfx->CreateBufferUAV(buffer, &view_desc);
				break;
			}
			case RGDescriptorType::RenderTarget:
			case RGDescriptorType::DepthStencil:
			default:
				ADRIA_ASSERT(false && "invalid resource view type for buffer");
			}
			buffer_view_map[res_id].emplace_back(view, type);
		}
	}

	RGTextureId RenderGraph::GetTextureId(RGResourceName name)
	{
		ADRIA_ASSERT(IsTextureDeclared(name));
		return texture_name_id_map[name];
	}

	RGBufferId RenderGraph::GetBufferId(RGResourceName name)
	{
		ADRIA_ASSERT(IsBufferDeclared(name));
		return buffer_name_id_map[name];
	}

	void RenderGraph::AddBufferBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= flags;
	}

	void RenderGraph::AddTextureBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= flags;
	}

	RGTextureCopySrcId RenderGraph::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::CopySource;
		}
		return RGTextureCopySrcId(handle);
	}

	RGTextureCopyDstId RenderGraph::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::CopyDest;
		}
		return RGTextureCopyDstId(handle);
	}

	RGBufferCopySrcId RenderGraph::ReadCopySrcBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferCopySrcId(handle);
	}

	RGBufferCopyDstId RenderGraph::WriteCopyDstBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferCopyDstId(handle);
	}

	RGBufferIndirectArgsId RenderGraph::ReadIndirectArgsBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferIndirectArgsId(handle);
	}

	RGBufferVertexId RenderGraph::ReadVertexBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferVertexId(handle);
	}

	RGBufferIndexId RenderGraph::ReadIndexBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferIndexId(handle);
	}

	RGBufferConstantId RenderGraph::ReadConstantBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferConstantId(handle);
	}

	RGRenderTargetId RenderGraph::RenderTarget(RGResourceName name, GfxTextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::RenderTarget;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::RenderTarget;
		}
		std::vector<std::pair<GfxTextureSubresourceDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::RenderTarget) return RGRenderTargetId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::RenderTarget);
		return RGRenderTargetId(view_id, handle);
	}

	RGDepthStencilId RenderGraph::DepthStencil(RGResourceName name, GfxTextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::DepthStencil;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::DepthWrite;
		}
		std::vector<std::pair<GfxTextureSubresourceDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::DepthStencil) return RGDepthStencilId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::DepthStencil);
		return RGDepthStencilId(view_id, handle);
	}

	RGTextureReadOnlyId RenderGraph::ReadTexture(RGResourceName name, GfxTextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::ShaderResource;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
		}
		std::vector<std::pair<GfxTextureSubresourceDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadOnly) return RGTextureReadOnlyId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadOnly);
		return RGTextureReadOnlyId(view_id, handle);
	}

	RGTextureReadWriteId RenderGraph::WriteTexture(RGResourceName name, GfxTextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::UnorderedAccess;
		}
		std::vector<std::pair<GfxTextureSubresourceDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite) return RGTextureReadWriteId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		return RGTextureReadWriteId(view_id, handle);
	}

	RGBufferReadOnlyId RenderGraph::ReadBuffer(RGResourceName name, GfxBufferSubresourceDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= GfxBindFlag::ShaderResource;
		std::vector<std::pair<GfxBufferSubresourceDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadOnly) return RGBufferReadOnlyId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadOnly);
		return RGBufferReadOnlyId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, GfxBufferSubresourceDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		std::vector<std::pair<GfxBufferSubresourceDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite) return RGBufferReadWriteId(i, handle);
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		return RGBufferReadWriteId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, RGResourceName counter_name, GfxBufferSubresourceDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		RGBufferId counter_handle = buffer_name_id_map[counter_name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		ADRIA_ASSERT(IsValidBufferHandle(counter_handle) && "Resource has not been declared!");

		RGBuffer* rg_buffer = GetRGBuffer(handle);
		RGBuffer* rg_counter_buffer = GetRGBuffer(counter_handle);

		rg_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		rg_counter_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;

		std::vector<std::pair<GfxBufferSubresourceDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (size_t i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite)
			{
				RGBufferReadWriteId rw_id(i, handle);
				if (auto it = buffer_uav_counter_map.find(rw_id); it != buffer_uav_counter_map.end())
				{
					if (it->second == counter_handle) return rw_id;
				}
			}
		}
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		RGBufferReadWriteId rw_id = RGBufferReadWriteId(view_id, handle);
		buffer_uav_counter_map.insert(std::make_pair(rw_id, counter_handle));
		return rw_id;
	}

	GfxTexture const& RenderGraph::GetCopySrcTexture(RGTextureCopySrcId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	GfxTexture& RenderGraph::GetCopyDstTexture(RGTextureCopyDstId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	GfxBuffer const& RenderGraph::GetCopySrcBuffer(RGBufferCopySrcId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer& RenderGraph::GetCopyDstBuffer(RGBufferCopyDstId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetVertexBuffer(RGBufferVertexId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetIndexBuffer(RGBufferIndexId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetConstantBuffer(RGBufferConstantId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxDescriptor RenderGraph::GetRenderTarget(RGRenderTargetId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetDepthStencil(RGDepthStencilId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadOnlyTexture(RGTextureReadOnlyId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadWriteTexture(RGTextureReadWriteId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		auto const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadWriteBuffer(RGBufferReadWriteId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		auto const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()].first;
	}

	void RenderGraph::DependencyLevel::AddPass(RenderGraphPassBase* pass)
	{
		passes.push_back(pass);
		texture_reads.insert(pass->texture_reads.begin(), pass->texture_reads.end());
		texture_writes.insert(pass->texture_writes.begin(), pass->texture_writes.end());
		buffer_reads.insert(pass->buffer_reads.begin(), pass->buffer_reads.end());
		buffer_writes.insert(pass->buffer_writes.begin(), pass->buffer_writes.end());
	}

	void RenderGraph::DependencyLevel::Setup()
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;

			texture_creates.insert(pass->texture_creates.begin(), pass->texture_creates.end());
			texture_destroys.insert(pass->texture_destroys.begin(), pass->texture_destroys.end());
			for (auto [resource, state] : pass->texture_state_map)
			{
				texture_state_map[resource] |= state;
			}

			buffer_creates.insert(pass->buffer_creates.begin(), pass->buffer_creates.end());
			buffer_destroys.insert(pass->buffer_destroys.begin(), pass->buffer_destroys.end());
			for (auto [resource, state] : pass->buffer_state_map)
			{
				buffer_state_map[resource] |= state;
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GfxDevice* gfx, GfxCommandList* cmd_list)
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;
			RenderGraphContext rg_resources(rg, *pass);
			if (pass->type == RGPassType::Graphics && !pass->SkipAutoRenderPassSetup())
			{
				GfxRenderPassDesc render_pass_desc{};
				if (pass->AllowUAVWrites()) render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
				else render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;

				for (auto const& render_target_info : pass->render_targets_info)
				{
					RGTextureId rt_texture = render_target_info.render_target_handle.GetResourceId();
					GfxTexture* texture = rg.GetTexture(rt_texture);

					GfxColorAttachmentDesc rtv_desc{};
					GfxTextureDesc const& desc = texture->GetDesc();
					GfxClearValue const& clear_value = desc.clear_value;
					if (clear_value.active_member != GfxClearValue::GfxActiveMember::None)
					{
						ADRIA_ASSERT(clear_value.active_member == GfxClearValue::GfxActiveMember::Color && "Invalid Clear Value for Render Target");
						rtv_desc.clear_value.Format = ConvertGfxFormat(desc.format);
						rtv_desc.clear_value.Color[0] = desc.clear_value.color.color[0];
						rtv_desc.clear_value.Color[1] = desc.clear_value.color.color[1];
						rtv_desc.clear_value.Color[2] = desc.clear_value.color.color[2];
						rtv_desc.clear_value.Color[3] = desc.clear_value.color.color[3];
					}
					rtv_desc.cpu_handle = rg.GetRenderTarget(render_target_info.render_target_handle);
					
					RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
					RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
					SplitAccessOp(render_target_info.render_target_access, load_access, store_access);

					switch (load_access)
					{
					case RGLoadAccessOp::Clear:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
						break;
					case RGLoadAccessOp::Discard:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
						break;
					case RGLoadAccessOp::Preserve: 
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
						break;
					case RGLoadAccessOp::NoAccess:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Load Access!");
					}

					switch (store_access)
					{
					case RGStoreAccessOp::Resolve:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
						break;
					case RGStoreAccessOp::Discard:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
						break;
					case RGStoreAccessOp::Preserve:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
						break;
					case RGStoreAccessOp::NoAccess:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Store Access!");
					}

					render_pass_desc.rtv_attachments.push_back(std::move(rtv_desc));
				}
				
				if (pass->depth_stencil.has_value())
				{
					auto depth_stencil_info = pass->depth_stencil.value();
					RGTextureId ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceId();
					GfxTexture* texture = rg.GetTexture(ds_texture);

					GfxDepthAttachmentDesc dsv_desc{};
					GfxTextureDesc const& desc = texture->GetDesc();
					GfxClearValue const& clear_value = desc.clear_value;
					if (clear_value.active_member != GfxClearValue::GfxActiveMember::None)
					{
						ADRIA_ASSERT(clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil && "Invalid Clear Value for Depth Stencil");
						dsv_desc.clear_value.Format = ConvertGfxFormat(desc.format);
						dsv_desc.clear_value.DepthStencil.Depth = desc.clear_value.depth_stencil.depth;
						dsv_desc.clear_value.DepthStencil.Stencil = desc.clear_value.depth_stencil.stencil;
					}
					dsv_desc.cpu_handle = rg.GetDepthStencil(depth_stencil_info.depth_stencil_handle);

					RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
					RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
					SplitAccessOp(depth_stencil_info.depth_access, load_access, store_access);

					switch (load_access)
					{
					case RGLoadAccessOp::Clear:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
						break;
					case RGLoadAccessOp::Discard:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
						break;
					case RGLoadAccessOp::Preserve:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
						break;
					case RGLoadAccessOp::NoAccess:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Load Access!");
					}

					switch (store_access)
					{
					case RGStoreAccessOp::Resolve:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
						break;
					case RGStoreAccessOp::Discard:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
						break;
					case RGStoreAccessOp::Preserve:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
						break;
					case RGStoreAccessOp::NoAccess:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Store Access!");
					}
					//todo add stencil
					render_pass_desc.dsv_attachment = std::move(dsv_desc);
				}
				ADRIA_ASSERT((pass->viewport_width != 0 && pass->viewport_height != 0) && "Viewport Width/Height is 0! The call to builder.SetViewport is probably missing...");
				render_pass_desc.width = pass->viewport_width;
				render_pass_desc.height = pass->viewport_height;

				PIXScopedEvent(cmd_list->GetNative(), PIX_COLOR_DEFAULT, pass->name.c_str());
				GPU_PROFILE_SCOPE(cmd_list->GetNative(), pass->name.c_str());
				cmd_list->BeginRenderPass(render_pass_desc, pass->UseLegacyRenderPasses());
				pass->Execute(rg_resources, gfx, cmd_list);
			}
			else
			{
				PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
				GPU_PROFILE_SCOPE(cmd_list, pass->name.c_str());
				pass->Execute(rg_resources, gfx, cmd_list);
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GfxDevice* gfx, std::span<GfxCommandList*> const& cmd_lists)
	{
		std::vector<std::future<void>> pass_futures;
		for (size_t k = 0; k < passes.size(); ++k)
		{
			pass_futures.push_back(TaskSystem::Submit([&](size_t j)
				{
				auto& pass = passes[j];
				GfxCommandList* cmd_list = cmd_lists[j];
				if (pass->IsCulled()) return;
				RenderGraphContext rg_resources(rg, *pass);
				if (pass->type == RGPassType::Graphics && !pass->SkipAutoRenderPassSetup())
				{
					GfxRenderPassDesc render_pass_desc{};
					if (pass->AllowUAVWrites()) render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
					else render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;

					for (auto const& render_target_info : pass->render_targets_info)
					{
						RGTextureId rt_texture = render_target_info.render_target_handle.GetResourceId();
						GfxTexture* texture = rg.GetTexture(rt_texture);

						GfxColorAttachmentDesc rtv_desc{};
						GfxTextureDesc const& desc = texture->GetDesc();
						GfxClearValue const& clear_value = desc.clear_value;
						if (clear_value.active_member != GfxClearValue::GfxActiveMember::None)
						{
							ADRIA_ASSERT(clear_value.active_member == GfxClearValue::GfxActiveMember::Color && "Invalid Clear Value for Render Target");
							rtv_desc.clear_value.Format = ConvertGfxFormat(desc.format);
							rtv_desc.clear_value.Color[0] = desc.clear_value.color.color[0];
							rtv_desc.clear_value.Color[1] = desc.clear_value.color.color[1];
							rtv_desc.clear_value.Color[2] = desc.clear_value.color.color[2];
							rtv_desc.clear_value.Color[3] = desc.clear_value.color.color[3];
						}
						rtv_desc.cpu_handle = rg.GetRenderTarget(render_target_info.render_target_handle);

						RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
						RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
						SplitAccessOp(render_target_info.render_target_access, load_access, store_access);

						switch (load_access)
						{
						case RGLoadAccessOp::Clear:
							rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
							break;
						case RGLoadAccessOp::Discard:
							rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
							break;
						case RGLoadAccessOp::Preserve:
							rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
							break;
						case RGLoadAccessOp::NoAccess:
							rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
							break;
						default:
							ADRIA_ASSERT(false && "Invalid Load Access!");
						}

						switch (store_access)
						{
						case RGStoreAccessOp::Resolve:
							rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
							break;
						case RGStoreAccessOp::Discard:
							rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
							break;
						case RGStoreAccessOp::Preserve:
							rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
							break;
						case RGStoreAccessOp::NoAccess:
							rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
							break;
						default:
							ADRIA_ASSERT(false && "Invalid Store Access!");
						}

						render_pass_desc.rtv_attachments.push_back(std::move(rtv_desc));
					}

					if (pass->depth_stencil.has_value())
					{
						auto depth_stencil_info = pass->depth_stencil.value();
						RGTextureId ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceId();
						GfxTexture* texture = rg.GetTexture(ds_texture);

						GfxDepthAttachmentDesc dsv_desc{};
						GfxTextureDesc const& desc = texture->GetDesc();
						GfxClearValue const& clear_value = desc.clear_value;
						if (clear_value.active_member != GfxClearValue::GfxActiveMember::None)
						{
							ADRIA_ASSERT(clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil && "Invalid Clear Value for Depth Stencil");
							dsv_desc.clear_value.Format = ConvertGfxFormat(desc.format);
							dsv_desc.clear_value.DepthStencil.Depth = desc.clear_value.depth_stencil.depth;
							dsv_desc.clear_value.DepthStencil.Stencil = desc.clear_value.depth_stencil.stencil;
						}
						dsv_desc.cpu_handle = rg.GetDepthStencil(depth_stencil_info.depth_stencil_handle);

						RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
						RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
						SplitAccessOp(depth_stencil_info.depth_access, load_access, store_access);

						switch (load_access)
						{
						case RGLoadAccessOp::Clear:
							dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
							break;
						case RGLoadAccessOp::Discard:
							dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
							break;
						case RGLoadAccessOp::Preserve:
							dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
							break;
						case RGLoadAccessOp::NoAccess:
							dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
							break;
						default:
							ADRIA_ASSERT(false && "Invalid Load Access!");
						}

						switch (store_access)
						{
						case RGStoreAccessOp::Resolve:
							dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
							break;
						case RGStoreAccessOp::Discard:
							dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
							break;
						case RGStoreAccessOp::Preserve:
							dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
							break;
						case RGStoreAccessOp::NoAccess:
							dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
							break;
						default:
							ADRIA_ASSERT(false && "Invalid Store Access!");
						}
						//todo add stencil
						render_pass_desc.dsv_attachment = std::move(dsv_desc);
					}
					ADRIA_ASSERT((pass->viewport_width != 0 && pass->viewport_height != 0) && "Viewport Width/Height is 0! The call to builder.SetViewport is probably missing...");
					render_pass_desc.width = pass->viewport_width;
					render_pass_desc.height = pass->viewport_height;
					PIXScopedEvent(cmd_list->GetNative(), PIX_COLOR_DEFAULT, pass->name.c_str());
					GPU_PROFILE_SCOPE(cmd_list->GetNative(), pass->name.c_str());
					cmd_list->BeginRenderPass(render_pass_desc, pass->UseLegacyRenderPasses());
					pass->Execute(rg_resources, gfx, cmd_list);
					cmd_list->EndRenderPass(pass->UseLegacyRenderPasses());
				}
				else
				{
					PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
					GPU_PROFILE_SCOPE(cmd_list, pass->name.c_str());
					pass->Execute(rg_resources, gfx, cmd_list);
				}
				}, k));
		}
		for (auto& future : pass_futures) future.wait();
	}

	size_t RenderGraph::DependencyLevel::GetSize() const
	{
		return passes.size();
	}

	size_t RenderGraph::DependencyLevel::GetNonCulledSize() const
	{
		return std::count_if(std::begin(passes), std::end(passes), [](auto* pass) {return !pass->IsCulled(); });
	}
}

