#include "D3D12CommandList.h"
#include "D3D12CommandSignature.h"
#include "D3D12Conversions.h"
#include "D3D12Device.h"
#include "D3D12CommandQueue.h"
#include "D3D12Buffer.h"
#include "D3D12Texture.h"
#include "D3D12QueryHeap.h"
#include "D3D12PipelineState.h"
#include "D3D12RayTracingPipeline.h"
#include "D3D12RayTracingShaderBindings.h"
#include "D3D12RingDescriptorAllocator.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxScopedEvent.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxNsightPerfManager.h"
#include "Utilities/StringConversions.h"
#include "pix3.h"

namespace adria
{
	namespace
	{
		constexpr D3D_PRIMITIVE_TOPOLOGY ToD3D12PrimitiveTopology(GfxPrimitiveTopology topology)
		{
			switch (topology)
			{
			case GfxPrimitiveTopology::PointList:
				return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			case GfxPrimitiveTopology::LineList:
				return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			case GfxPrimitiveTopology::LineStrip:
				return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case GfxPrimitiveTopology::TriangleList:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			case GfxPrimitiveTopology::TriangleStrip:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			default:
				if (topology >= GfxPrimitiveTopology::PatchList1 && topology <= GfxPrimitiveTopology::PatchList32)
					return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + ((Uint32)topology - (Uint32)GfxPrimitiveTopology::PatchList1));
				else return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			}
		}
		constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE ToD3D12RenderPassBeginningAccess(GfxLoadAccessOp op)
		{
			switch (op)
			{
			case GfxLoadAccessOp::Discard:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			case GfxLoadAccessOp::Preserve:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			case GfxLoadAccessOp::Clear:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			case GfxLoadAccessOp::NoAccess:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
			}
			ADRIA_UNREACHABLE();
		}
		constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ToD3D12RenderPassEndingAccess(GfxStoreAccessOp op)
		{
			switch (op)
			{
			case GfxStoreAccessOp::Discard:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
			case GfxStoreAccessOp::Preserve:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			case GfxStoreAccessOp::Resolve:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
			case GfxStoreAccessOp::NoAccess:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			ADRIA_UNREACHABLE();
		}
		constexpr D3D12_RENDER_PASS_FLAGS ToD3D12RenderPassFlags(GfxRenderPassFlags flags)
		{
			D3D12_RENDER_PASS_FLAGS d3d12_flags = D3D12_RENDER_PASS_FLAG_NONE;
			if (flags & GfxRenderPassFlagBit_ReadOnlyDepth) d3d12_flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH;
			if (flags & GfxRenderPassFlagBit_ReadOnlyStencil) d3d12_flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_STENCIL;
			if (flags & GfxRenderPassFlagBit_AllowUAVWrites) d3d12_flags |= D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
			if (flags & GfxRenderPassFlagBit_SuspendingPass) d3d12_flags |= D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS;
			if (flags & GfxRenderPassFlagBit_ResumingPass) d3d12_flags |= D3D12_RENDER_PASS_FLAG_RESUMING_PASS;
			return d3d12_flags;
		}
		constexpr D3D12_COMMAND_LIST_TYPE ToGfxCommandListType(GfxCommandListType type)
		{
			switch (type)
			{
			case GfxCommandListType::Graphics:
				return D3D12_COMMAND_LIST_TYPE_DIRECT;
			case GfxCommandListType::Compute:
				return D3D12_COMMAND_LIST_TYPE_COMPUTE;
			case GfxCommandListType::Copy:
				return D3D12_COMMAND_LIST_TYPE_COPY;
			}
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		}
		constexpr D3D12_QUERY_TYPE ToD3D12QueryType(GfxQueryType query_type)
		{
			switch (query_type)
			{
			case GfxQueryType::Timestamp:
				return D3D12_QUERY_TYPE_TIMESTAMP;
			case GfxQueryType::Occlusion:
				return D3D12_QUERY_TYPE_OCCLUSION;
			case GfxQueryType::BinaryOcclusion:
				return D3D12_QUERY_TYPE_BINARY_OCCLUSION;
			case GfxQueryType::PipelineStatistics:
				return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
			}
			return D3D12_QUERY_TYPE_TIMESTAMP;
		}
		void ToD3D12ClearValue(GfxClearValue value, D3D12_CLEAR_VALUE& d3d12_clear_value)
		{
			d3d12_clear_value.Format = ConvertGfxFormat(value.format);
			if (value.active_member == GfxClearValue::GfxActiveMember::Color)
			{
				memcpy(d3d12_clear_value.Color, value.color.color, sizeof(Float) * 4);
			}
			else if (value.active_member == GfxClearValue::GfxActiveMember::DepthStencil)
			{
				d3d12_clear_value.DepthStencil.Depth = value.depth_stencil.depth;
				d3d12_clear_value.DepthStencil.Stencil = value.depth_stencil.stencil;
			}
		}
	}

	D3D12CommandList::D3D12CommandList(GfxDevice* gfx, GfxCommandListType type, Char const* name)
		: gfx((D3D12Device*)gfx), type(type), cmd_queue(gfx->GetCommandQueue(type)), use_legacy_barriers(!gfx->GetCapabilities().SupportsEnhancedBarriers()), current_rt_bindings(nullptr)
	{

		draw_indirect_signature = std::make_unique<DrawIndirectSignature>(gfx);
		draw_indexed_indirect_signature = std::make_unique<DrawIndexedIndirectSignature>(gfx);
		dispatch_indirect_signature = std::make_unique<DispatchIndirectSignature>(gfx);
		if (gfx->GetCapabilities().SupportsMeshShaders())
		{
			dispatch_mesh_indirect_signature = std::make_unique<DispatchMeshIndirectSignature>(gfx);
		}

		D3D12_COMMAND_LIST_TYPE cmd_list_type = ToGfxCommandListType(type);
		ID3D12Device* device = (ID3D12Device*)gfx->GetNative();
		HRESULT hr = device->CreateCommandAllocator(cmd_list_type, IID_PPV_ARGS(cmd_allocator.GetAddressOf()));
		GFX_CHECK_CALL(hr);
		hr = device->CreateCommandList(0, cmd_list_type, cmd_allocator, nullptr, IID_PPV_ARGS(cmd_list.GetAddressOf()));
		GFX_CHECK_CALL(hr);
		cmd_list->SetName(ToWideString(name).c_str());
		cmd_list->Close();
	}

	D3D12CommandList::~D3D12CommandList() {}

	GfxDevice* D3D12CommandList::GetDevice()
	{
		return gfx;
	}

	void* D3D12CommandList::GetNative() const
	{
		return cmd_list.Get();
	}

	GfxCommandQueue* D3D12CommandList::GetQueue() const
	{
		return cmd_queue;
	}

	void D3D12CommandList::ResetAllocator()
	{
		cmd_allocator->Reset();
	}

	void D3D12CommandList::Begin()
	{
		cmd_list->Reset(cmd_allocator.Get(), nullptr);
		ResetState();
	}

	void D3D12CommandList::End()
	{
		FlushBarriers();
		cmd_list->Close();
	}

	void D3D12CommandList::Wait(GfxFence& fence, Uint64 value)
	{
		pending_waits.emplace_back(fence, value);
	}

	void D3D12CommandList::Signal(GfxFence& fence, Uint64 value)
	{
		pending_signals.emplace_back(fence, value);
	}

	void D3D12CommandList::WaitAll()
	{
		for (Uint64 i = 0; i < pending_waits.size(); ++i)
		{
			cmd_queue->Wait(pending_waits[i].first, pending_waits[i].second);
		}
		pending_waits.clear();
	}

	void D3D12CommandList::Submit()
	{
		WaitAll();
		GfxCommandList* cmd_list_array[] = { this };
		cmd_queue->ExecuteCommandLists(cmd_list_array);
		SignalAll();
	}

	void D3D12CommandList::SignalAll()
	{
		for (Uint64 i = 0; i < pending_signals.size(); ++i)
		{
			cmd_queue->Signal(pending_signals[i].first, pending_signals[i].second);
		}
		pending_signals.clear();
	}

	void D3D12CommandList::ResetState()
	{
		current_pso = nullptr;
		current_render_pass = nullptr;
		current_state_object = nullptr;
		current_primitive_topology = GfxPrimitiveTopology::Undefined;
		current_stencil_ref = 0;
		current_rt_bindings.reset();
		current_context = Context::Invalid;

		if (type == GfxCommandListType::Graphics || type == GfxCommandListType::Compute)
		{
			D3D12OnlineDescriptorAllocator* descriptor_allocator = gfx->GetDescriptorAllocator();
			if (descriptor_allocator)
			{
				ID3D12DescriptorHeap* heaps[] = { descriptor_allocator->GetHeap()->GetD3D12Heap() };
				cmd_list->SetDescriptorHeaps(1, heaps);

				ID3D12RootSignature* common_rs = gfx->GetCommonRootSignature();
				cmd_list->SetComputeRootSignature(common_rs);
				if (type == GfxCommandListType::Graphics)
				{
					cmd_list->SetGraphicsRootSignature(common_rs);
				}
			}
		}
	}

	void D3D12CommandList::BeginEvent(Char const* event_name)
	{
		BeginEvent(event_name, GfxEventColor(0xFF, 0xB3, 0x5E));
	}

	void D3D12CommandList::BeginEvent(Char const* event_name, Uint32 event_color)
	{
		PIXBeginEvent(cmd_list.Get(), event_color, event_name);
		g_GfxProfiler.BeginProfileScope(this, event_name);
		if (GfxNsightPerfManager* nsight_perf_manager = gfx->GetNsightPerfManager())
		{
			nsight_perf_manager->PushRange(this, event_name);
		}
	}

	void D3D12CommandList::EndEvent()
	{
		if (GfxNsightPerfManager* nsight_perf_manager = gfx->GetNsightPerfManager())
		{
			nsight_perf_manager->PopRange(this);
		}
		g_GfxProfiler.EndProfileScope(this);
		PIXEndEvent(cmd_list.Get());
	}

	void D3D12CommandList::BeginQuery(GfxQueryHeap& query_heap, Uint32 index)
	{
		D3D12_QUERY_TYPE d3d12_query_type = ToD3D12QueryType(query_heap.GetDesc().type);
		cmd_list->EndQuery((ID3D12QueryHeap*)query_heap.GetHandle(), d3d12_query_type, index);
	}

	void D3D12CommandList::EndQuery(GfxQueryHeap& query_heap, Uint32 index)
	{
		D3D12_QUERY_TYPE d3d12_query_type = ToD3D12QueryType(query_heap.GetDesc().type);
		cmd_list->EndQuery((ID3D12QueryHeap*)query_heap.GetHandle(), d3d12_query_type, index);
	}

	void D3D12CommandList::ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset)
	{
		cmd_list->ResolveQueryData((ID3D12QueryHeap*)query_heap.GetHandle(), ToD3D12QueryType(query_heap.GetDesc().type), start, count, (ID3D12Resource*)dst_buffer.GetNative(), dst_offset);
	}

	void D3D12CommandList::Draw(Uint32 vertex_count, Uint32 instance_count, Uint32 start_vertex_location, Uint32 start_instance_location)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		if (vertex_count == 0 || instance_count == 0)
		{
			return;
		}
		cmd_list->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
	}

	void D3D12CommandList::DrawIndexed(Uint32 index_count, Uint32 instance_count, Uint32 index_offset, Uint32 base_vertex_location, Uint32 start_instance_location)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		if (index_count == 0 || instance_count == 0)
		{
			return;
		}
		cmd_list->DrawIndexedInstanced(index_count, instance_count, index_offset, base_vertex_location, start_instance_location);
	}

	void D3D12CommandList::Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
	{
		ADRIA_ASSERT(current_context == Context::Compute);
		if (group_count_x == 0 || group_count_y == 0 || group_count_z == 0)
		{
			return;
		}
		cmd_list->Dispatch(group_count_x, group_count_y, group_count_z);
	}

	void D3D12CommandList::DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		if (group_count_x == 0 || group_count_y == 0 || group_count_z == 0)
		{
			return;
		}
		cmd_list->DispatchMesh(group_count_x, group_count_y, group_count_z);
	}

	void D3D12CommandList::DrawIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(draw_indirect_signature->Get(), 1, (ID3D12Resource*)buffer.GetNative(), offset, nullptr, 0);
	}

	void D3D12CommandList::DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(draw_indexed_indirect_signature->Get(), 1, (ID3D12Resource*)buffer.GetNative(), offset, nullptr, 0);
	}

	void D3D12CommandList::DispatchIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Compute);
		cmd_list->ExecuteIndirect(dispatch_indirect_signature->Get(), 1, (ID3D12Resource*)buffer.GetNative(), offset, nullptr, 0);
	}

	void D3D12CommandList::DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(dispatch_mesh_indirect_signature->Get(), 1, (ID3D12Resource*)buffer.GetNative(), offset, nullptr, 0);
	}

	GfxRayTracingShaderBindings* D3D12CommandList::BeginRayTracingShaderBindings(GfxRayTracingPipeline const* pipeline)
	{
		ADRIA_ASSERT(pipeline != nullptr);
		ADRIA_ASSERT(pipeline->IsValid());

		D3D12RayTracingPipeline const* d3d12_pipeline = static_cast<D3D12RayTracingPipeline const*>(pipeline);
		ID3D12StateObject* state_object = d3d12_pipeline->GetD3D12StateObject();
		cmd_list->SetPipelineState1(state_object);
		current_rt_bindings = std::make_unique<D3D12RayTracingShaderBindings>(d3d12_pipeline);
		return current_rt_bindings.get();
	}

	void D3D12CommandList::DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth)
	{
		ADRIA_ASSERT(current_context == Context::Compute);
		if (dispatch_width == 0 || dispatch_height == 0 || dispatch_depth == 0)
		{
			return;
		}
		ADRIA_ASSERT(current_rt_bindings != nullptr);

		D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
		current_rt_bindings->CommitWithAllocator(*gfx->GetDynamicAllocator(), dispatch_desc);
		dispatch_desc.Width = dispatch_width;
		dispatch_desc.Height = dispatch_height;
		dispatch_desc.Depth = dispatch_depth;
		cmd_list->DispatchRays(&dispatch_desc);
		current_rt_bindings.reset();
	}

	void D3D12CommandList::TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = (ID3D12Resource*)texture.GetNative();
				legacy_barriers.push_back(barrier);
			}
			else
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = (ID3D12Resource*)texture.GetNative();
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.StateBefore = ToD3D12LegacyResourceState(flags_before);
				barrier.Transition.StateAfter = ToD3D12LegacyResourceState(flags_after);
				legacy_barriers.push_back(barrier);
			}
		}
		else
		{
			D3D12_TEXTURE_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			barrier.LayoutBefore = ToD3D12BarrierLayout(flags_before);
			barrier.LayoutAfter = ToD3D12BarrierLayout(flags_after);
			barrier.pResource = (ID3D12Resource*)texture.GetNative();
			barrier.Subresources = CD3DX12_BARRIER_SUBRESOURCE_RANGE(subresource);

			if (HasAnyFlag(flags_before, GfxResourceState::Discard))
			{
				barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
			}
			texture_barriers.push_back(barrier);
		}
	}

	void D3D12CommandList::BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = (ID3D12Resource*)buffer.GetNative();
				legacy_barriers.push_back(barrier);
			}
			else
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = (ID3D12Resource*)buffer.GetNative();
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.StateBefore = ToD3D12LegacyResourceState(flags_before);
				barrier.Transition.StateAfter = ToD3D12LegacyResourceState(flags_after);
				legacy_barriers.push_back(barrier);
			}
		}
		else
		{
			D3D12_BUFFER_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			barrier.pResource = (ID3D12Resource*)buffer.GetNative();
			barrier.Offset = 0;
			barrier.Size = UINT64_MAX;

			buffer_barriers.push_back(barrier);
		}
	}

	void D3D12CommandList::GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = nullptr;
				legacy_barriers.push_back(barrier);
			}
			else
			{
				ADRIA_ASSERT_MSG(false, "Unsupported flags for legacy barriers!");
			}
		}
		else
		{
			D3D12_GLOBAL_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			global_barriers.push_back(barrier);
		}
	}

	void D3D12CommandList::FlushBarriers()
	{
		if (use_legacy_barriers)
		{
			if (!legacy_barriers.empty())
			{
				cmd_list->ResourceBarrier((Uint32)legacy_barriers.size(), legacy_barriers.data());
				legacy_barriers.clear();
			}
		}
		else
		{
			std::vector<D3D12_BARRIER_GROUP> barrier_groups;
			barrier_groups.reserve(3);

			if (!texture_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((Uint32)texture_barriers.size(), texture_barriers.data()));
			}
			if (!buffer_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((Uint32)buffer_barriers.size(), buffer_barriers.data()));
			}
			if (!global_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((Uint32)global_barriers.size(), global_barriers.data()));
			}

			if (!barrier_groups.empty())
			{
				cmd_list->Barrier((Uint32)barrier_groups.size(), barrier_groups.data());
			}

			texture_barriers.clear();
			buffer_barriers.clear();
			global_barriers.clear();
		}
	}

	void D3D12CommandList::CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size)
	{
		cmd_list->CopyBufferRegion((ID3D12Resource*)dst.GetNative(), dst_offset, (ID3D12Resource*)src.GetNative(), src_offset, size);
	}

	void D3D12CommandList::CopyBuffer(GfxBuffer& dst, GfxBuffer const& src)
	{
		cmd_list->CopyResource((ID3D12Resource*)dst.GetNative(), (ID3D12Resource*)src.GetNative());
	}

	void D3D12CommandList::CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
	{
		D3D12_TEXTURE_COPY_LOCATION dst_texture;
		dst_texture.pResource = (ID3D12Resource*)dst.GetNative();
		dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_texture.SubresourceIndex = dst_mip + dst.GetDesc().mip_levels * dst_array;

		D3D12_TEXTURE_COPY_LOCATION src_texture;
		src_texture.pResource = (ID3D12Resource*)src.GetNative();
		src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_texture.SubresourceIndex = src_mip + src.GetDesc().mip_levels * src_array;

		cmd_list->CopyTextureRegion(&dst_texture, 0, 0, 0, &src_texture, nullptr);
	}

	void D3D12CommandList::CopyTexture(GfxTexture& dst, GfxTexture const& src)
	{
		ADRIA_ASSERT(dst.GetWidth() == src.GetWidth());
		ADRIA_ASSERT(dst.GetHeight() == src.GetHeight());
		cmd_list->CopyResource((ID3D12Resource*)dst.GetNative(), (ID3D12Resource*)src.GetNative());
	}

	void D3D12CommandList::CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
	{
		GfxTextureDesc const& desc = src.GetDesc();

		D3D12_TEXTURE_COPY_LOCATION dst_texture;
		dst_texture.pResource = (ID3D12Resource*)dst.GetNative();
		dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_texture.PlacedFootprint.Offset = dst_offset;
		dst_texture.PlacedFootprint.Footprint.Width = desc.width;
		dst_texture.PlacedFootprint.Footprint.Depth = 1;
		dst_texture.PlacedFootprint.Footprint.Height = desc.height;
		dst_texture.PlacedFootprint.Footprint.Format = ConvertGfxFormat(desc.format);
		dst_texture.PlacedFootprint.Footprint.RowPitch = (Uint32)AlignUp(GetRowPitch(desc.format, dst_texture.PlacedFootprint.Footprint.Width), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION src_texture;
		src_texture.pResource = (ID3D12Resource*)src.GetNative();
		src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_texture.SubresourceIndex = src_mip + src.GetDesc().mip_levels * src_array;

		cmd_list->CopyTextureRegion(&dst_texture, (Uint32)dst_offset, 0, 0, &src_texture, nullptr);
	}

	void D3D12CommandList::CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset)
	{
		GfxTextureDesc const& desc = dst_texture.GetDesc();

		Uint32 min_width = GetGfxFormatBlockSize(desc.format);
		Uint32 min_height = GetGfxFormatBlockSize(desc.format);
		Uint32 w = std::max(desc.width >> mip_level, min_width);
		Uint32 h = std::max(desc.height >> mip_level, min_height);
		Uint32 d = std::max(desc.depth >> mip_level, 1u);

		D3D12_TEXTURE_COPY_LOCATION copy_dst{};
		copy_dst.pResource = (ID3D12Resource*)dst_texture.GetNative();
		copy_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		copy_dst.SubresourceIndex = mip_level + desc.mip_levels * array_slice;

		D3D12_TEXTURE_COPY_LOCATION copy_src = {};
		copy_src.pResource = (ID3D12Resource*)src_buffer.GetNative();
		copy_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		copy_src.PlacedFootprint.Offset = offset;
		copy_src.PlacedFootprint.Footprint.Format = ConvertGfxFormat(desc.format);
		copy_src.PlacedFootprint.Footprint.Width = w;
		copy_src.PlacedFootprint.Footprint.Height = h;
		copy_src.PlacedFootprint.Footprint.Depth = d;
		copy_src.PlacedFootprint.Footprint.RowPitch = dst_texture.GetRowPitch(mip_level);
		cmd_list->CopyTextureRegion(&copy_dst, 0, 0, 0, &copy_src, nullptr);
	}

	void D3D12CommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Float const clear_value[4])
	{
		D3D12Descriptor uav_cpu = gfx->CreateBufferViewImpl(&resource, GfxSubresourceType::UAV, uav_desc);
		D3D12Descriptor uav_gpu = gfx->GetDescriptorAllocator()->Allocate(1);
		gfx->GetD3D12Device()->CopyDescriptorsSimple(1, ToD3D12CPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->ClearUnorderedAccessViewFloat(ToD3D12GPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), (ID3D12Resource*)resource.GetNative(), clear_value, 0, nullptr);
		gfx->FreeDescriptorImpl(uav_cpu, GfxDescriptorType::CBV_SRV_UAV);
	}
	void D3D12CommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Float const clear_value[4])
	{
		D3D12Descriptor uav_cpu = gfx->CreateTextureViewImpl(&resource, GfxSubresourceType::UAV, uav_desc);
		D3D12Descriptor uav_gpu = gfx->GetDescriptorAllocator()->Allocate(1);
		gfx->GetD3D12Device()->CopyDescriptorsSimple(1, ToD3D12CPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->ClearUnorderedAccessViewFloat(ToD3D12GPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), (ID3D12Resource*)resource.GetNative(), clear_value, 0, nullptr);
		gfx->FreeDescriptorImpl(uav_cpu, GfxDescriptorType::CBV_SRV_UAV);
	}
	void D3D12CommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
	{
		D3D12Descriptor uav_cpu = gfx->CreateBufferViewImpl(&resource, GfxSubresourceType::UAV, uav_desc);
		D3D12Descriptor uav_gpu = gfx->GetDescriptorAllocator()->Allocate(1);
		gfx->GetD3D12Device()->CopyDescriptorsSimple(1, ToD3D12CPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->ClearUnorderedAccessViewUint(ToD3D12GPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), (ID3D12Resource*)resource.GetNative(), clear_value, 0, nullptr);
		gfx->FreeDescriptorImpl(uav_cpu, GfxDescriptorType::CBV_SRV_UAV);
	}
	void D3D12CommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
	{
		D3D12Descriptor uav_cpu = gfx->CreateTextureViewImpl(&resource, GfxSubresourceType::UAV, uav_desc);
		D3D12Descriptor uav_gpu = gfx->GetDescriptorAllocator()->Allocate(1);
		gfx->GetD3D12Device()->CopyDescriptorsSimple(1, ToD3D12CPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->ClearUnorderedAccessViewUint(ToD3D12GPUHandle(uav_gpu), ToD3D12CPUHandle(uav_cpu), (ID3D12Resource*)resource.GetNative(), clear_value, 0, nullptr);
		gfx->FreeDescriptorImpl(uav_cpu, GfxDescriptorType::CBV_SRV_UAV);
	}

	void D3D12CommandList::WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data)
	{
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter{};
		parameter.Dest = buffer.GetGpuAddress() + offset;
		parameter.Value = data;
		cmd_list->WriteBufferImmediate(1, &parameter, nullptr);
	}

	void D3D12CommandList::BeginRenderPass(GfxRenderPassDesc const& render_pass_desc)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		ADRIA_ASSERT(current_render_pass == nullptr);
		current_render_pass = &render_pass_desc;
		if (!render_pass_desc.legacy)
		{
			std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtvs{};
			std::unique_ptr<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> dsv = nullptr;
			for (GfxColorAttachmentDesc const& attachment : render_pass_desc.rtv_attachments)
			{
				D3D12_RENDER_PASS_RENDER_TARGET_DESC rtv_desc{};
				rtv_desc.cpuDescriptor = DecodeToD3D12CPUHandle(attachment.cpu_handle);
				rtv_desc.BeginningAccess = { ToD3D12RenderPassBeginningAccess(attachment.beginning_access) };
				ToD3D12ClearValue(attachment.clear_value, rtv_desc.BeginningAccess.Clear.ClearValue);
				rtv_desc.EndingAccess = { ToD3D12RenderPassEndingAccess(attachment.ending_access), {} };
				rtvs.push_back(rtv_desc);
			}

			if (render_pass_desc.dsv_attachment)
			{
				GfxDepthAttachmentDesc const& _dsv_desc = render_pass_desc.dsv_attachment.value();
				dsv = std::make_unique<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC>();

				dsv->cpuDescriptor = DecodeToD3D12CPUHandle(_dsv_desc.cpu_handle);
				dsv->DepthBeginningAccess = { ToD3D12RenderPassBeginningAccess(_dsv_desc.depth_beginning_access) };
				ToD3D12ClearValue(_dsv_desc.clear_value, dsv->DepthBeginningAccess.Clear.ClearValue);
				dsv->StencilBeginningAccess = { ToD3D12RenderPassBeginningAccess(_dsv_desc.stencil_beginning_access) };
				ToD3D12ClearValue(_dsv_desc.clear_value, dsv->StencilBeginningAccess.Clear.ClearValue);
				dsv->DepthEndingAccess = { ToD3D12RenderPassEndingAccess(_dsv_desc.depth_ending_access), {} };
				dsv->StencilEndingAccess = { ToD3D12RenderPassEndingAccess(_dsv_desc.stencil_ending_access), {} };

			}

			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* _dsv = dsv.get();
			D3D12_RENDER_PASS_FLAGS flags = ToD3D12RenderPassFlags(render_pass_desc.flags);
			cmd_list->BeginRenderPass(static_cast<Uint32>(rtvs.size()), rtvs.data(), _dsv, flags);
		}
		else
		{
			std::vector<GfxDescriptor> rtv_handles{};
			GfxDescriptor const* dsv_handle = nullptr;

			for (GfxColorAttachmentDesc const& rtv : render_pass_desc.rtv_attachments)
			{
				rtv_handles.push_back(rtv.cpu_handle);
				if (rtv.beginning_access == GfxLoadAccessOp::Clear)
				{
					ClearRenderTarget(rtv.cpu_handle, rtv.clear_value.color.color);
				}
			}

			if (render_pass_desc.dsv_attachment.has_value())
			{
				dsv_handle = &render_pass_desc.dsv_attachment->cpu_handle;
				GfxClearValue::GfxClearDepthStencil depth_stencil = render_pass_desc.dsv_attachment->clear_value.depth_stencil;
				if (render_pass_desc.dsv_attachment->depth_beginning_access == GfxLoadAccessOp::Clear)
				{
					ClearDepth(*dsv_handle, depth_stencil.depth, depth_stencil.stencil, false);
				}
			}
			SetRenderTargets(rtv_handles, dsv_handle);
		}
		SetViewport(0, 0, current_render_pass->width, current_render_pass->height);
	}

	void D3D12CommandList::EndRenderPass()
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		ADRIA_ASSERT(current_render_pass != nullptr);
		if (current_render_pass && !current_render_pass->legacy)
		{
			cmd_list->EndRenderPass();
		}
		current_render_pass = nullptr;
	}

	void D3D12CommandList::SetPipelineState(GfxPipelineState const* pso)
	{
		if (pso != current_pso)
		{
			current_pso = pso;
			if (pso == nullptr)
			{
				cmd_list->SetPipelineState(nullptr);
			}
			else
			{
				cmd_list->SetPipelineState((ID3D12PipelineState*)pso->GetNative());
				if (pso->GetType() == GfxPipelineStateType::Graphics || pso->GetType() == GfxPipelineStateType::MeshShader)
				{
					ADRIA_ASSERT(current_context == Context::Graphics);
				}
				else
				{
					ADRIA_ASSERT(current_context == Context::Compute);
				}
			}
		}
	}

	void D3D12CommandList::SetStencilReference(Uint8 stencil_ref)
	{
		if (stencil_ref != current_stencil_ref)
		{
			cmd_list->OMSetStencilRef(stencil_ref);
			current_stencil_ref = stencil_ref;
		}
	}

	void D3D12CommandList::SetBlendFactor(Float const* blend_factor)
	{
		cmd_list->OMSetBlendFactor(blend_factor);
	}

	void D3D12CommandList::SetPrimitiveTopology(GfxPrimitiveTopology topology)
	{
		if (topology != current_primitive_topology)
		{
			cmd_list->IASetPrimitiveTopology(ToD3D12PrimitiveTopology(topology));
			current_primitive_topology = topology;
		}
	}

	void D3D12CommandList::SetIndexBuffer(GfxIndexBufferView* index_buffer_view)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		if (index_buffer_view)
		{
			D3D12_INDEX_BUFFER_VIEW ibv{};
			ibv.BufferLocation = index_buffer_view->buffer_location;
			ibv.SizeInBytes = index_buffer_view->size_in_bytes;
			ibv.Format = ConvertGfxFormat(index_buffer_view->format);
			cmd_list->IASetIndexBuffer(&ibv);
		}
		else
		{
			cmd_list->IASetIndexBuffer(nullptr);
		}
	}

	void D3D12CommandList::SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot)
	{
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = vertex_buffer_view.buffer_location;
		vbv.SizeInBytes = vertex_buffer_view.size_in_bytes;
		vbv.StrideInBytes = vertex_buffer_view.stride_in_bytes;
		cmd_list->IASetVertexBuffers(start_slot, 1, &vbv);
	}

	void D3D12CommandList::SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		std::vector<D3D12_VERTEX_BUFFER_VIEW> vbs(vertex_buffer_views.size());
		for (Uint64 i = 0; i < vertex_buffer_views.size(); ++i)
		{
			vbs[i].BufferLocation = vertex_buffer_views[i].buffer_location;
			vbs[i].SizeInBytes = vertex_buffer_views[i].size_in_bytes;
			vbs[i].StrideInBytes = vertex_buffer_views[i].stride_in_bytes;
		}
		cmd_list->IASetVertexBuffers(start_slot, (Uint32)vbs.size(), vbs.data());
	}

	void D3D12CommandList::SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		D3D12_VIEWPORT vp = { (Float)x, (Float)y, (Float)width, (Float)height, 0.0f, 1.0f };
		cmd_list->RSSetViewports(1, &vp);
		SetScissorRect(x, y, width, height);
	}

	void D3D12CommandList::SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		D3D12_RECT rect = { (LONG)x, (LONG)y, LONG(x + width), LONG(y + height) };
		cmd_list->RSSetScissorRects(1, &rect);
	}

	void D3D12CommandList::SetShadingRate(GfxShadingRate shading_rate)
	{
		GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, GfxShadingRateCombiner::Passthrough };
		SetShadingRate(shading_rate, combiners);
	}

	void D3D12CommandList::SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners)
	{
		D3D12_SHADING_RATE_COMBINER d3d12_combiners[SHADING_RATE_COMBINER_COUNT] = {};
		for (Uint32 i = 0; i < SHADING_RATE_COMBINER_COUNT; ++i)
		{
			d3d12_combiners[i] = ToD3D12ShadingRateCombiner(combiners[i]);
		}
		cmd_list->RSSetShadingRate(ToD3D12ShadingRate(shading_rate), d3d12_combiners);
	}

	void D3D12CommandList::SetShadingRateImage(GfxTexture const* texture)
	{
		cmd_list->RSSetShadingRateImage(texture ? (ID3D12Resource*)texture->GetNative() : nullptr);
	}

	void D3D12CommandList::BeginVRS(GfxShadingRateInfo const& info)
	{
		if (info.shading_mode != GfxVariableShadingMode::None)
		{
			if (info.shading_mode == GfxVariableShadingMode::Image && info.shading_rate_image != nullptr &&
				info.shading_rate_combiner != GfxShadingRateCombiner::Passthrough)
			{
				GfxTexture* vrs_image = info.shading_rate_image;
				TextureBarrier(*vrs_image, GfxResourceState::ComputeUAV, GfxResourceState::ShadingRate);
				FlushBarriers();

				GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, info.shading_rate_combiner };
				SetShadingRate(info.shading_rate, combiners);
				SetShadingRateImage(vrs_image);
			}
			else
			{
				GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, info.shading_rate_combiner };
				SetShadingRate(info.shading_rate, combiners);
			}
		}
	}

	void D3D12CommandList::EndVRS(GfxShadingRateInfo const& info)
	{
		if (info.shading_mode != GfxVariableShadingMode::None)
		{
			if (info.shading_mode == GfxVariableShadingMode::Image && info.shading_rate_image != nullptr &&
				info.shading_rate_combiner != GfxShadingRateCombiner::Passthrough)
			{
				GfxTexture* vrs_image = info.shading_rate_image;
				TextureBarrier(*vrs_image, GfxResourceState::ShadingRate, GfxResourceState::ComputeUAV);
				FlushBarriers();
			}

			GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, GfxShadingRateCombiner::Passthrough };
			SetShadingRate(GfxShadingRate_1X1, combiners);
			SetShadingRateImage(nullptr);
		}
	}

	void D3D12CommandList::SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRoot32BitConstant(slot, data, offset);
		}
		else
		{
			cmd_list->SetComputeRoot32BitConstant(slot, data, offset);
		}
	}

	void D3D12CommandList::SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRoot32BitConstants(slot, data_size / sizeof(Uint32), data, offset);
		}
		else
		{
			cmd_list->SetComputeRoot32BitConstants(slot, data_size / sizeof(Uint32), data, offset);
		}
	}

	void D3D12CommandList::SetRootCBV(Uint32 slot, void const* data, Uint64 data_size)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		GfxLinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();
		GfxDynamicAllocation alloc = dynamic_allocator->Allocate(data_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		alloc.Update(data, data_size);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootConstantBufferView(slot, alloc.gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootConstantBufferView(slot, alloc.gpu_address);
		}
	}

	void D3D12CommandList::SetRootCBV(Uint32 slot, Uint64 gpu_address)
	{
		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootConstantBufferView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootConstantBufferView(slot, gpu_address);
		}
	}

	void D3D12CommandList::SetRootSRV(Uint32 slot, Uint64 gpu_address)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootShaderResourceView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootShaderResourceView(slot, gpu_address);
		}
	}

	void D3D12CommandList::SetRootUAV(Uint32 slot, Uint64 gpu_address)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootUnorderedAccessView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootUnorderedAccessView(slot, gpu_address);
		}
	}

	void D3D12CommandList::SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor)
	{
		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootDescriptorTable(slot, DecodeToD3D12GPUHandle(base_descriptor));
		}
		else
		{
			cmd_list->SetComputeRootDescriptorTable(slot, DecodeToD3D12GPUHandle(base_descriptor));
		}
	}

	GfxDynamicAllocation D3D12CommandList::AllocateTransient(Uint32 size, Uint32 align)
	{
		return gfx->GetDynamicAllocator()->Allocate(size, align);
	}

	void D3D12CommandList::ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color)
	{
		cmd_list->ClearRenderTargetView(DecodeToD3D12CPUHandle(rtv), clear_color, 0, nullptr);
	}

	void D3D12CommandList::ClearDepth(GfxDescriptor dsv, Float depth, Uint8 stencil, Bool clear_stencil)
	{
		D3D12_CLEAR_FLAGS d3d12_clear_flags = D3D12_CLEAR_FLAG_DEPTH;
		if (clear_stencil)
		{
			d3d12_clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
		}
		cmd_list->ClearDepthStencilView(DecodeToD3D12CPUHandle(dsv), d3d12_clear_flags, depth, stencil, 0, nullptr);
	}

	void D3D12CommandList::SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv, Bool single_rt)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* d3d12_dsv = nullptr;
		if (dsv)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE _d3d12_dsv = DecodeToD3D12CPUHandle(*dsv);
			d3d12_dsv = &_d3d12_dsv;
		}
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> d3d12_rtvs(rtvs.size());
		for (Uint64 i = 0; i < d3d12_rtvs.size(); ++i)
		{
			d3d12_rtvs[i] = DecodeToD3D12CPUHandle(rtvs[i]);
		}
		cmd_list->OMSetRenderTargets((Uint32)d3d12_rtvs.size(), d3d12_rtvs.data(), single_rt, d3d12_dsv);
	}

	void D3D12CommandList::SetContext(Context ctx)
	{
		current_context = ctx;
	}
}

