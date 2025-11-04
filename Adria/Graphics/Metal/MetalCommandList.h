#pragma once
#include "Graphics/GfxCommandList.h"

@protocol MTLCommandBuffer;
@protocol MTLRenderCommandEncoder;
@protocol MTLComputeCommandEncoder;
@protocol MTLBlitCommandEncoder;

namespace adria
{
    class MetalCommandList final : public GfxCommandList
    {
    public:
        explicit MetalCommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, Char const* name = "");
        virtual ~MetalCommandList() override;

        virtual GfxDevice* GetDevice() override;
        virtual void* GetNative() const override;
        virtual GfxCommandQueue* GetQueue() const override { return nullptr; }

        virtual void ResetAllocator() override {}
        virtual void Begin() override;
        virtual void End() override;
        virtual void Wait(GfxFence& fence, Uint64 value) override {}
        virtual void Signal(GfxFence& fence, Uint64 value) override {}
        virtual void WaitAll() override {}
        virtual void Submit() override;
        virtual void SignalAll() override {}
        virtual void ResetState() override;

        virtual void BeginEvent(Char const* event_name) override;
        virtual void BeginEvent(Char const* event_name, Uint32 event_color) override;
        virtual void EndEvent() override;

        virtual void BeginQuery(GfxQueryHeap& query_heap, Uint32 index) override {}
        virtual void EndQuery(GfxQueryHeap& query_heap, Uint32 index) override {}
        virtual void ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset) override {}

        virtual void Draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 start_vertex_location = 0, Uint32 start_instance_location = 0) override;
        virtual void DrawIndexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 index_offset = 0, Uint32 base_vertex_location = 0, Uint32 start_instance_location = 0) override;
        virtual void Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override;
        virtual void DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1) override {}
        virtual void DrawIndirect(GfxBuffer const& buffer, Uint32 offset) override {}
        virtual void DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset) override {}
        virtual void DispatchIndirect(GfxBuffer const& buffer, Uint32 offset) override {}
        virtual void DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset) override {}
        virtual void DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth = 1) override {}

        virtual void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource = 0) override {}
        virtual void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after) override {}
        virtual void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after) override {}
        virtual void FlushBarriers() override {}

        virtual void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src) override;
        virtual void CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size) override;
        virtual void CopyTexture(GfxTexture& dst, GfxTexture const& src) override {}
        virtual void CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override {}
        virtual void CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array) override {}
        virtual void CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset) override {}

        virtual void ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Float const clear_value[4]) override {}
        virtual void ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Float const clear_value[4]) override {}
        virtual void ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Uint32 const clear_value[4]) override {}
        virtual void ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Uint32 const clear_value[4]) override {}
        virtual void WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data) override {}

        virtual void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc) override;
        virtual void EndRenderPass() override;

        virtual void SetPipelineState(GfxPipelineState const* state) override;
        virtual GfxRayTracingShaderBindings* BeginRayTracingShaderBindings(GfxRayTracingPipeline const* pipeline) override { return nullptr; }
        virtual void SetStencilReference(Uint8 stencil) override;
        virtual void SetBlendFactor(Float const* blend_factor) override {}
        virtual void SetPrimitiveTopology(GfxPrimitiveTopology topology) override;
        virtual void SetIndexBuffer(GfxIndexBufferView* index_buffer_view) override;
        virtual void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot = 0) override;
        virtual void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot = 0) override;
        virtual void SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;
        virtual void SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height) override;

        virtual void SetShadingRate(GfxShadingRate shading_rate) override {}
        virtual void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners) override {}
        virtual void SetShadingRateImage(GfxTexture const* texture) override {}
        virtual void BeginVRS(GfxShadingRateInfo const& info) override {}
        virtual void EndVRS(GfxShadingRateInfo const& info) override {}

        virtual void SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset = 0) override {}
        virtual void SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset = 0) override {}
        virtual void SetRootCBV(Uint32 slot, void const* data, Uint64 data_size) override {}
        virtual void SetRootCBV(Uint32 slot, Uint64 gpu_address) override {}
        virtual void SetRootSRV(Uint32 slot, Uint64 gpu_address) override {}
        virtual void SetRootUAV(Uint32 slot, Uint64 gpu_address) override {}
        virtual void SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor) override {}

        virtual void BuildRayTracingTLAS(GfxRayTracingTLAS& tlas) override {}
        virtual void BuildRayTracingBLAS(GfxRayTracingBLAS& blas) override {}
        virtual void UpdateRayTracingTLAS(GfxRayTracingTLAS& tlas, std::span<GfxRayTracingInstance> instances) override {}
        virtual void UpdateRayTracingBLAS(GfxRayTracingBLAS& blas, std::span<GfxRayTracingGeometry> geometries) override {}

        id<MTLCommandBuffer> GetCommandBuffer() const { return command_buffer; }

    private:
        GfxDevice* gfx;
        GfxCommandListType type;
        id<MTLCommandBuffer> command_buffer;
        id<MTLRenderCommandEncoder> render_encoder;
        id<MTLComputeCommandEncoder> compute_encoder;
        id<MTLBlitCommandEncoder> blit_encoder;
        GfxPrimitiveTopology current_topology;
        GfxPipelineState const* current_pipeline_state;
    };
}
