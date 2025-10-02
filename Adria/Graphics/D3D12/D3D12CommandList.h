#pragma once
#include "Graphics/GfxCommandList.h"

namespace adria
{
	class D3D12CommandList : public IGfxCommandList
	{
	public:
		explicit D3D12CommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, Char const* name = "");
		~D3D12CommandList();
		ADRIA_NONCOPYABLE(D3D12CommandList)
			ADRIA_DEFAULT_MOVABLE(D3D12CommandList)

			GfxDevice* GetDevice() const override;
		void* GetNative() const override;
		IGfxCommandQueue* GetQueue() const override;

		void ResetAllocator() override;
		void Begin() override;
		void End() override;
		void Wait(GfxFence& fence, Uint64 value) override;
		void Signal(GfxFence& fence, Uint64 value) override;
		void WaitAll() override;
		void Submit() override;
		void SignalAll() override;
		void ResetState() override;
		void SetHeap(GfxOnlineDescriptorAllocator* heap) override;
		void ResetHeap() override;

		void BeginEvent(Char const* event_name) override;
		void BeginEvent(Char const* event_name, Uint32 event_color) override;
		void EndEvent() override;

		void BeginQuery(GfxQueryHeap& query_heap, Uint32 index) override;
		void EndQuery(GfxQueryHeap& query_heap, Uint32 index) override;
		void ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset) override;

		void Draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 start_vertex_location = 0, Uint32 start_instance_location = 0) override;
		void DrawIndexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 index_offset = 0, Uint32 base_vertex_location = 0, Uint32 start_instance_location = 0) override;
		void Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override;
		void DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override;
		void DrawIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		void DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		void DispatchIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		void DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		void DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth = 1) override;

		void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) override;
		void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after) override;
		void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after) override;
		void FlushBarriers() override;

		void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src) override;
		void CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size) override;
		void CopyTexture(GfxTexture& dst, GfxTexture const& src) override;
		void CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override;
		void CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override;
		void CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset) override;

		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Float const* clear_value) override;
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Float const* clear_value) override;
		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Uint32 const* clear_value) override;
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Uint32 const* clear_value) override;
		void WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data) override;

		void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc) override;
		void EndRenderPass() override;

		void SetPipelineState(GfxPipelineState const* state) override;
		GfxRayTracingShaderTable& SetStateObject(GfxStateObject const* state_object) override;

		void SetStencilReference(Uint8 stencil) override;
		void SetBlendFactor(Float const* blend_factor) override;
		void SetPrimitiveTopology(GfxPrimitiveTopology topology) override;
		void SetIndexBuffer(GfxIndexBufferView* index_buffer_view) override;
		void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot = 0) override;
		void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot = 0) override;
		void SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;
		void SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;

		void SetShadingRate(GfxShadingRate shading_rate) override;
		void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners) override;
		void SetShadingRateImage(GfxTexture const* texture) override;
		void BeginVRS(GfxShadingRateInfo const& info) override;
		void EndVRS(GfxShadingRateInfo const& info) override;

		void SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset = 0) override;
		void SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset = 0) override;
		void SetRootCBV(Uint32 slot, void const* data, Uint64 data_size) override;
		void SetRootCBV(Uint32 slot, Uint64 gpu_address) override;
		void SetRootSRV(Uint32 slot, Uint64 gpu_address) override;
		void SetRootUAV(Uint32 slot, Uint64 gpu_address) override;
		void SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor) override;

		GfxDynamicAllocation AllocateTransient(Uint32 size, Uint32 align = 0) override;

		void ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color) override;
		void ClearDepth(GfxDescriptor dsv, Float depth = 1.0f, Uint8 stencil = 0, Bool clear_stencil = false) override;
		void SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv = nullptr, Bool single_rt = false) override;

		void SetContext(Context ctx) override;

	private:
		GfxDevice* gfx = nullptr;
		GfxCommandListType type;
		GfxCommandQueue* cmd_queue;
		Ref<ID3D12GraphicsCommandList7> cmd_list = nullptr;
		Ref<ID3D12CommandAllocator> cmd_allocator = nullptr;

		GfxPipelineState const* current_pso = nullptr;
		GfxRenderPassDesc const* current_render_pass = nullptr;

		ID3D12StateObject* current_state_object = nullptr;
		std::unique_ptr<GfxRayTracingShaderTable> current_rt_table;

		GfxPrimitiveTopology current_primitive_topology = GfxPrimitiveTopology::Undefined;
		Uint8 current_stencil_ref = 0;

		Context current_context = Context::Invalid;

		std::vector<std::pair<GfxFence&, Uint64>> pending_waits;
		std::vector<std::pair<GfxFence&, Uint64>> pending_signals;

		Bool use_legacy_barriers = false;
		std::vector<D3D12_TEXTURE_BARRIER> texture_barriers;
		std::vector<D3D12_BUFFER_BARRIER> buffer_barriers;
		std::vector<D3D12_GLOBAL_BARRIER> global_barriers;
		std::vector<D3D12_RESOURCE_BARRIER> legacy_barriers;
	};
}