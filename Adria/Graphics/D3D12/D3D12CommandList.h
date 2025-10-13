#pragma once
#include "D3D12CommandSignature.h"
#include "Graphics/GfxCommandList.h"

namespace adria
{
	class D3D12Device;
	class D3D12CommandList final : public GfxCommandList
	{
	public:
		explicit D3D12CommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, Char const* name = "");
		virtual ~D3D12CommandList() override;
		ADRIA_NONCOPYABLE(D3D12CommandList)
		ADRIA_DEFAULT_MOVABLE(D3D12CommandList)

		virtual GfxDevice* GetDevice() override;
		virtual void* GetNative() const override;
		virtual GfxCommandQueue* GetQueue() const override;

		virtual void ResetAllocator() override;
		virtual void Begin() override;
		virtual void End() override;
		virtual void Wait(GfxFence& fence, Uint64 value) override;
		virtual void Signal(GfxFence& fence, Uint64 value) override;
		virtual void WaitAll() override;
		virtual void Submit() override;
		virtual void SignalAll() override;
		virtual void ResetState() override;
		virtual void SetHeap(GfxOnlineDescriptorAllocator* heap) override;
		virtual void ResetHeap() override;

		virtual void BeginEvent(Char const* event_name) override;
		virtual void BeginEvent(Char const* event_name, Uint32 event_color) override;
		virtual void EndEvent() override;

		virtual void BeginQuery(GfxQueryHeap& query_heap, Uint32 index) override;
		virtual void EndQuery(GfxQueryHeap& query_heap, Uint32 index) override;
		virtual void ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset) override;

		virtual void Draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 start_vertex_location = 0, Uint32 start_instance_location = 0) override;
		virtual void DrawIndexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 index_offset = 0, Uint32 base_vertex_location = 0, Uint32 start_instance_location = 0) override;
		virtual void Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override;
		virtual void DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override;
		virtual void DrawIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		virtual void DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		virtual void DispatchIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		virtual void DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset) override;
		virtual void DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth = 1) override;

		virtual void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) override;
		virtual void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after) override;
		virtual void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after) override;
		virtual void FlushBarriers() override;

		virtual void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src) override;
		virtual void CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size) override;
		virtual void CopyTexture(GfxTexture& dst, GfxTexture const& src) override;
		virtual void CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override;
		virtual void CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override;
		virtual void CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset) override;

		virtual void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Float const* clear_value) override;
		virtual void ClearUAV(GfxTexture const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Float const* clear_value) override;
		virtual void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Uint32 const* clear_value) override;
		virtual void ClearUAV(GfxTexture const& resource, GfxDescriptor uav_gpu, GfxDescriptor uav_cpu, Uint32 const* clear_value) override;
		virtual void WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data) override;

		virtual void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc) override;
		virtual void EndRenderPass() override;

		virtual void SetPipelineState(GfxPipelineState const* state) override;
		virtual GfxRayTracingShaderTable& SetStateObject(GfxStateObject const* state_object) override;

		virtual void SetStencilReference(Uint8 stencil) override;
		virtual void SetBlendFactor(Float const* blend_factor) override;
		virtual void SetPrimitiveTopology(GfxPrimitiveTopology topology) override;
		virtual void SetIndexBuffer(GfxIndexBufferView* index_buffer_view) override;
		virtual void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot = 0) override;
		virtual void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot = 0) override;
		virtual void SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;
		virtual void SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;

		virtual void SetShadingRate(GfxShadingRate shading_rate) override;
		virtual void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners) override;
		virtual void SetShadingRateImage(GfxTexture const* texture) override;
		virtual void BeginVRS(GfxShadingRateInfo const& info) override;
		virtual void EndVRS(GfxShadingRateInfo const& info) override;

		virtual void SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset = 0) override;
		virtual void SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset = 0) override;
		virtual void SetRootCBV(Uint32 slot, void const* data, Uint64 data_size) override;
		virtual void SetRootCBV(Uint32 slot, Uint64 gpu_address) override;
		virtual void SetRootSRV(Uint32 slot, Uint64 gpu_address) override;
		virtual void SetRootUAV(Uint32 slot, Uint64 gpu_address) override;
		virtual void SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor) override;

		virtual GfxDynamicAllocation AllocateTransient(Uint32 size, Uint32 align = 0) override;

		virtual void ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color) override;
		virtual void ClearDepth(GfxDescriptor dsv, Float depth = 1.0f, Uint8 stencil = 0, Bool clear_stencil = false) override;
		virtual void SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv = nullptr, Bool single_rt = false) override;

		virtual void SetContext(Context ctx) override;

	private:
		D3D12Device* gfx = nullptr;
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

		std::unique_ptr<DrawIndirectSignature> draw_indirect_signature;
		std::unique_ptr<DrawIndexedIndirectSignature> draw_indexed_indirect_signature;
		std::unique_ptr<DispatchIndirectSignature> dispatch_indirect_signature;
		std::unique_ptr<DispatchMeshIndirectSignature> dispatch_mesh_indirect_signature;
	};
}