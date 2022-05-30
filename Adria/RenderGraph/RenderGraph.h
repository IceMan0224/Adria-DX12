#pragma once
#include <stack>
#include <array>
#include "RenderGraphBlackboard.h"
#include "RenderGraphPass.h"
#include "RenderGraphResources.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/DescriptorHeap.h"
#include "../Graphics/Heap.h"
#include "../Utilities/HashUtil.h"


namespace adria
{
	class RenderGraphResourcePool
	{
		struct PooledTexture
		{
			std::unique_ptr<Texture> texture;
			uint64 last_used_frame;
		};

	public:
		explicit RenderGraphResourcePool(GraphicsDevice* device) : device(device) {}

		void Tick()
		{
			for (size_t i = 0; i < texture_pool.size();)
			{
				PooledTexture& resource = texture_pool[i].first;
				bool active = texture_pool[i].second;
				if (!active && resource.last_used_frame + 3 < frame_index)
				{
					std::swap(texture_pool[i], texture_pool.back());
					texture_pool.pop_back();
				}
				else ++i;
			}
			++frame_index;
		}
		Texture* AllocateTexture(TextureDesc const& desc)
		{
			for (auto& [pool_texture, active] : texture_pool)
			{
				if (!active && pool_texture.texture->GetDesc() == desc)
				{
					pool_texture.last_used_frame = frame_index;
					active = true;
					return pool_texture.texture.get();
				}
			}
			return texture_pool.emplace_back(std::pair{ PooledTexture{ std::make_unique<Texture>(device, desc), frame_index}, true}).first.texture.get();
		}
		void ReleaseTexture(Texture* texture)
		{
			for (auto& [pooled_texture, active] : texture_pool)
			{
				auto& texture_ptr = pooled_texture.texture;
				if (active && texture_ptr.get() == texture)
				{
					active = false;
				}
			}
		}

		GraphicsDevice* GetDevice() const { return device; }
	private:
		GraphicsDevice* device = nullptr;
		uint64 frame_index = 0;
		std::vector<std::pair<PooledTexture, bool>> texture_pool;
	};
	using RGResourcePool = RenderGraphResourcePool;

	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		RGTextureRef CreateTexture(char const* name, TextureDesc const& desc);

		RGTextureRef Read(RGTextureRef handle, ERGReadAccess read_flag = ReadAccess_PixelShader);
		RGTextureRef Write(RGTextureRef handle, ERGWriteAccess write_flag = WriteAccess_Unordered);
		RGTextureRef RenderTarget(RGTextureRTVRef rtv_handle, ERGLoadStoreAccessOp load_store_op);
		RGTextureRef DepthStencil(RGTextureDSVRef dsv_handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly = false, 
			ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess);

		RGTextureSRVRef CreateSRV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureUAVRef CreateUAV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureRTVRef CreateRTV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureDSVRef CreateDSV(RGTextureRef handle, TextureViewDesc const& desc = {});

		void SetViewport(uint32 width, uint32 height);
	private:
		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
	};
	using RGBuilder = RenderGraphBuilder;

	class RenderGraph
	{
		friend class RenderGraphBuilder;
		friend class RenderGraphResources;

		class DependencyLevel
		{
			friend RenderGraph;
		public:

			explicit DependencyLevel(RenderGraph& rg) : rg(rg) {}
			void AddPass(RenderGraphPassBase* pass);
			void Setup();
			void Execute(GraphicsDevice* gfx, CommandList* cmd_list);
			size_t GetSize() const;
			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*>   passes;
			std::unordered_set<RGTextureRef> creates;
			std::unordered_set<RGTextureRef> reads;
			std::unordered_set<RGTextureRef> writes;
			std::unordered_set<RGTextureRef> destroys;
			std::unordered_map<RGTextureRef, ResourceState> required_states;
		};

	public:

		RenderGraph(RGResourcePool& pool) : pool(pool), gfx(pool.GetDevice())
		{}
		RenderGraph(RenderGraph const&) = delete;
		RenderGraph(RenderGraph&&) = default;
		RenderGraph& operator=(RenderGraph const&) = delete;
		RenderGraph& operator=(RenderGraph&&) = default;
		~RenderGraph()
		{
			for (auto& [_, view] : texture_srv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto& [_, view] : texture_uav_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto& [_, view] : texture_rtv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			for (auto& [_, view] : texture_dsv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		template<typename PassData, typename... Args> requires std::is_constructible_v<RenderGraphPass<PassData>, Args...>
		[[maybe_unused]] PassData const& AddPass(Args&&... args)
		{
			passes.emplace_back(std::make_unique<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			RenderGraphBuilder builder(*this, *passes.back());
			passes.back()->Setup(builder);
			return (*dynamic_cast<RenderGraphPass<PassData>*>(passes.back().get())).GetPassData();
		}

		RGTextureRef CreateTexture(char const* name, TextureDesc const& desc);
		RGTextureRef ImportTexture(char const* name, Texture* texture);
		Texture* GetTexture(RGTextureRef) const;

		RGBufferRef CreateBuffer(char const* name, BufferDesc const& desc);
		Buffer* GetBuffer(RGBufferRef) const;

		void Build();
		void Execute();

		bool IsValidTextureHandle(RGTextureRef) const;
		bool IsValidBufferHandle(RGBufferRef) const;

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		RGResourcePool& pool;
		GraphicsDevice* gfx;
		RGBlackboard blackboard;

		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<std::unique_ptr<RGBuffer>> buffers;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		mutable std::unordered_map<RGTextureRef, std::vector<TextureViewDesc>> view_desc_map;
		mutable std::unordered_map<RGTextureSRVRef, ResourceView> texture_srv_cache;
		mutable std::unordered_map<RGTextureUAVRef, ResourceView> texture_uav_cache;
		mutable std::unordered_map<RGTextureRTVRef, ResourceView> texture_rtv_cache;
		mutable std::unordered_map<RGTextureDSVRef, ResourceView> texture_dsv_cache;

	private:
		RGTexture* GetRGTexture(RGTextureRef handle) const;
		RGBuffer* GetRGBuffer(RGBufferRef handle) const;

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGTextureSRVRef CreateSRV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureUAVRef CreateUAV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureRTVRef CreateRTV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureDSVRef CreateDSV(RGTextureRef handle, TextureViewDesc const& desc);

		ResourceView GetSRV(RGTextureSRVRef handle) const;
		ResourceView GetUAV(RGTextureUAVRef handle) const;
		ResourceView GetRTV(RGTextureRTVRef handle) const;
		ResourceView GetDSV(RGTextureDSVRef handle) const;
	};

}
