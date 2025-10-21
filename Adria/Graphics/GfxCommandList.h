#pragma once
#include "GfxDescriptor.h"
#include "GfxResource.h"
#include "GfxDynamicAllocation.h"
#include "GfxShadingRate.h"
#include "GfxStates.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;
	class GfxFence;
	class GfxBuffer;
	class GfxTexture;
	class GfxPipelineState;
	class GfxQueryHeap;
	class GfxRayTracingPipeline;
	class GfxRayTracingShaderBindings;
	struct GfxVertexBufferView;
	struct GfxIndexBufferView;
	struct GfxRenderPassDesc;
	struct GfxShadingRateInfo;
	struct GfxBufferDescriptorDesc;
	struct GfxTextureDescriptorDesc;

	enum class GfxCommandListType : Uint8
	{
		Graphics,
		Compute,
		Copy
	};

	class GfxCommandList
	{
	public:
		enum class Context
		{
			Invalid,
			Graphics,
			Compute
		};

	public:
		virtual ~GfxCommandList() = default;

		virtual GfxDevice* GetDevice() = 0;
		virtual void* GetNative() const = 0;
		virtual GfxCommandQueue* GetQueue() const = 0;

		virtual void ResetAllocator() = 0;
		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual void Wait(GfxFence& fence, Uint64 value) = 0;
		virtual void Signal(GfxFence& fence, Uint64 value) = 0;
		virtual void WaitAll() = 0;
		virtual void Submit() = 0;
		virtual void SignalAll() = 0;
		virtual void ResetState() = 0;

		virtual void BeginEvent(Char const* event_name) = 0;
		virtual void BeginEvent(Char const* event_name, Uint32 event_color) = 0;
		virtual void EndEvent() = 0;

		virtual void BeginQuery(GfxQueryHeap& query_heap, Uint32 index) = 0;
		virtual void EndQuery(GfxQueryHeap& query_heap, Uint32 index) = 0;
		virtual void ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset) = 0;

		virtual void Draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 start_vertex_location = 0, Uint32 start_instance_location = 0) = 0;
		virtual void DrawIndexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 index_offset = 0, Uint32 base_vertex_location = 0, Uint32 start_instance_location = 0) = 0;
		virtual void Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) = 0;
		virtual void DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) = 0;
		virtual void DrawIndirect(GfxBuffer const& buffer, Uint32 offset) = 0;
		virtual void DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset) = 0;
		virtual void DispatchIndirect(GfxBuffer const& buffer, Uint32 offset) = 0;
		virtual void DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset) = 0;
		virtual void DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth = 1) = 0;

		virtual void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource = static_cast<Uint32>(-1)) = 0;
		virtual void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after) = 0;
		virtual void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after) = 0;
		virtual void FlushBarriers() = 0;

		virtual void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src) = 0;
		virtual void CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size) = 0;
		virtual void CopyTexture(GfxTexture& dst, GfxTexture const& src) = 0;
		virtual void CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) = 0;
		virtual void CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) = 0;
		virtual void CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset) = 0;

		virtual void ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Float const clear_value[4]) = 0;
		virtual void ClearTexture(GfxTexture const& resource,	GfxTextureDescriptorDesc const& uav_desc, Float const clear_value[4]) = 0;
		virtual void ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Uint32 const clear_value[4]) = 0;
		virtual void ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Uint32 const clear_value[4]) = 0;
		virtual void WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data) = 0;

		virtual void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc) = 0;
		virtual void EndRenderPass() = 0;

		virtual void SetPipelineState(GfxPipelineState const* state) = 0;
		virtual GfxRayTracingShaderBindings* BeginRayTracingShaderBindings(GfxRayTracingPipeline const* pipeline) = 0;
		virtual void SetStencilReference(Uint8 stencil) = 0;
		virtual void SetBlendFactor(Float const* blend_factor) = 0;
		virtual void SetPrimitiveTopology(GfxPrimitiveTopology topology) = 0;
		virtual void SetIndexBuffer(GfxIndexBufferView* index_buffer_view) = 0;
		virtual void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot = 0) = 0;
		virtual void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot = 0) = 0;
		virtual void SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height) = 0;
		virtual void SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height) = 0;

		virtual void SetShadingRate(GfxShadingRate shading_rate) = 0;
		virtual void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners) = 0;
		virtual void SetShadingRateImage(GfxTexture const* texture) = 0;
		virtual void BeginVRS(GfxShadingRateInfo const& info) = 0;
		virtual void EndVRS(GfxShadingRateInfo const& info) = 0;

		virtual void SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset = 0) = 0;
		virtual void SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset = 0) = 0;
		template<typename T>
		void SetRootConstants(Uint32 slot, T const& data)
		{
			SetRootConstants(slot, &data, sizeof(T));
		}
		virtual void SetRootCBV(Uint32 slot, void const* data, Uint64 data_size) = 0;
		template<typename T>
		void SetRootCBV(Uint32 slot, T const& data)
		{
			SetRootCBV(slot, &data, sizeof(T));
		}
		virtual void SetRootCBV(Uint32 slot, Uint64 gpu_address) = 0;
		virtual void SetRootSRV(Uint32 slot, Uint64 gpu_address) = 0;
		virtual void SetRootUAV(Uint32 slot, Uint64 gpu_address) = 0;
		virtual void SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor) = 0;
		virtual GfxDynamicAllocation AllocateTransient(Uint32 size, Uint32 align = 0) = 0;

		virtual void ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color) = 0;
		virtual void ClearDepth(GfxDescriptor dsv, Float depth = 1.0f, Uint8 stencil = 0, Bool clear_stencil = false) = 0;
		virtual void SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv = nullptr, Bool single_rt = false) = 0;

		virtual void SetContext(Context ctx) = 0;

		void ClearBuffer(GfxBuffer const& resource, Float const clear_value[4]);
		void ClearTexture(GfxTexture const& resource, Float const clear_value[4]);
		void ClearBuffer(GfxBuffer const& resource, Uint32 const clear_value[4]);
		void ClearTexture(GfxTexture const& resource, Uint32 const clear_value[4]);
	};
}