#import <Metal/Metal.h>
#include "MetalCommandList.h"
#include "MetalDevice.h"
#include "MetalBuffer.h"
#include "MetalTexture.h"
#include "MetalPipelineState.h"
#include "MetalConversions.h"
#include "MetalRayTracingPipeline.h"
#include "MetalRayTracingShaderBindings.h"
#include "MetalArgumentBuffer.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxBufferView.h"

namespace adria
{
    constexpr Uint32 BINDLESS_ARGUMENT_BUFFER_SLOT = 30;

    static MTLLoadAction ConvertLoadAction(GfxLoadAccessOp op)
    {
        switch (op)
        {
        case GfxLoadAccessOp::Discard: return MTLLoadActionDontCare;
        case GfxLoadAccessOp::Preserve: return MTLLoadActionLoad;
        case GfxLoadAccessOp::Clear: return MTLLoadActionClear;
        case GfxLoadAccessOp::NoAccess: return MTLLoadActionDontCare;
        default: return MTLLoadActionDontCare;
        }
    }

    static MTLStoreAction ConvertStoreAction(GfxStoreAccessOp op)
    {
        switch (op)
        {
        case GfxStoreAccessOp::Discard: return MTLStoreActionDontCare;
        case GfxStoreAccessOp::Preserve: return MTLStoreActionStore;
        case GfxStoreAccessOp::Resolve: return MTLStoreActionMultisampleResolve;
        case GfxStoreAccessOp::NoAccess: return MTLStoreActionDontCare;
        default: return MTLStoreActionStore;
        }
    }

    static MTLPrimitiveType ConvertTopology(GfxPrimitiveTopology topology)
    {
        switch (topology)
        {
        case GfxPrimitiveTopology::PointList: return MTLPrimitiveTypePoint;
        case GfxPrimitiveTopology::LineList: return MTLPrimitiveTypeLine;
        case GfxPrimitiveTopology::LineStrip: return MTLPrimitiveTypeLineStrip;
        case GfxPrimitiveTopology::TriangleList: return MTLPrimitiveTypeTriangle;
        case GfxPrimitiveTopology::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        default: return MTLPrimitiveTypeTriangle;
        }
    }

    static MTLCullMode ConvertCullMode(GfxCullMode mode)
    {
        switch (mode)
        {
        case GfxCullMode::None: return MTLCullModeNone;
        case GfxCullMode::Front: return MTLCullModeFront;
        case GfxCullMode::Back: return MTLCullModeBack;
        default: return MTLCullModeNone;
        }
    }

    MetalCommandList::MetalCommandList(GfxDevice* gfx, GfxCommandListType type, Char const* name)
        : metal_device(static_cast<MetalDevice*>(gfx)), type(type), command_buffer(nil), render_encoder(nil),
          compute_encoder(nil), blit_encoder(nil),
          current_topology(GfxPrimitiveTopology::TriangleList),
          current_pipeline_state(nullptr), current_index_buffer_view(nullptr)
    {
    }

    MetalCommandList::~MetalCommandList()
    {
        @autoreleasepool
        {
            render_encoder = nil;
            compute_encoder = nil;
            blit_encoder = nil;
            command_buffer = nil;
        }
    }

    GfxDevice* MetalCommandList::GetDevice()
    {
        return metal_device;
    }

    void* MetalCommandList::GetNative() const
    {
        return (__bridge void*)command_buffer;
    }

    void MetalCommandList::Begin()
    {
        id<MTLCommandQueue> queue = metal_device->GetMTLCommandQueue();
        command_buffer = [queue commandBuffer];
    }

    void MetalCommandList::End()
    {
        if (render_encoder)
        {
            [render_encoder endEncoding];
            render_encoder = nil;
        }
        if (compute_encoder)
        {
            [compute_encoder endEncoding];
            compute_encoder = nil;
        }
        if (blit_encoder)
        {
            [blit_encoder endEncoding];
            blit_encoder = nil;
        }
    }

    void MetalCommandList::Submit()
    {
        if (command_buffer)
        {
            [command_buffer commit];
        }
    }

    void MetalCommandList::ResetState()
    {
        current_topology = GfxPrimitiveTopology::TriangleList;
        current_pipeline_state = nullptr;
        current_index_buffer_view = nullptr;
    }

    void MetalCommandList::BeginEvent(Char const* event_name)
    {
        [command_buffer pushDebugGroup:[NSString stringWithUTF8String:event_name]];
    }

    void MetalCommandList::BeginEvent(Char const* event_name, Uint32 event_color)
    {
        BeginEvent(event_name);
    }

    void MetalCommandList::EndEvent()
    {
        [command_buffer popDebugGroup];
    }

    void MetalCommandList::Draw(Uint32 vertex_count, Uint32 instance_count, Uint32 start_vertex_location, Uint32 start_instance_location)
    {
        if (render_encoder)
        {
            [render_encoder drawPrimitives:ConvertTopology(current_topology)
                                vertexStart:start_vertex_location
                                vertexCount:vertex_count
                                instanceCount:instance_count
                                baseInstance:start_instance_location];
        }
    }

    void MetalCommandList::DrawIndexed(Uint32 index_count, Uint32 instance_count, Uint32 index_offset, Uint32 base_vertex_location, Uint32 start_instance_location)
    {
        if (render_encoder && current_index_buffer_view)
        {
            MetalDevice::BufferLookupResult lookup = metal_device->LookupBuffer(current_index_buffer_view->buffer_location);
            if (lookup.buffer != nil)
            {
                MTLIndexType index_type = ConvertIndexFormat(current_index_buffer_view->format);
                Uint32 index_size = (index_type == MTLIndexTypeUInt16) ? 2 : 4;

                [render_encoder drawIndexedPrimitives:ConvertTopology(current_topology)
                                           indexCount:index_count
                                            indexType:index_type
                                          indexBuffer:lookup.buffer
                                    indexBufferOffset:lookup.offset + (index_offset * index_size)
                                        instanceCount:instance_count
                                           baseVertex:base_vertex_location
                                         baseInstance:start_instance_location];
            }
        }
    }

    void MetalCommandList::Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
    {
        if (compute_encoder && current_pipeline_state && current_pipeline_state->GetType() == GfxPipelineStateType::Compute)
        {
            MetalComputePipelineState const* metal_pso = static_cast<MetalComputePipelineState const*>(current_pipeline_state);
            MTLSize threadgroups = MTLSizeMake(group_count_x, group_count_y, group_count_z);
            MTLSize threadsPerThreadgroup = metal_pso->GetThreadsPerThreadgroup();
            [compute_encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerThreadgroup];
        }
    }

    void MetalCommandList::DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
    {
        if (render_encoder && current_pipeline_state && current_pipeline_state->GetType() == GfxPipelineStateType::MeshShader)
        {
            MetalMeshShadingPipelineState const* metal_pso = static_cast<MetalMeshShadingPipelineState const*>(current_pipeline_state);

            MTLSize threadgroups = MTLSizeMake(group_count_x, group_count_y, group_count_z);
            MTLSize threadsPerObjectThreadgroup = metal_pso->GetThreadsPerObjectThreadgroup();
            MTLSize threadsPerMeshThreadgroup = metal_pso->GetThreadsPerMeshThreadgroup();

            [render_encoder drawMeshThreadgroups:threadgroups
                          threadsPerObjectThreadgroup:threadsPerObjectThreadgroup
                           threadsPerMeshThreadgroup:threadsPerMeshThreadgroup];
        }
    }

    void MetalCommandList::CopyBuffer(GfxBuffer& dst, GfxBuffer const& src)
    {
        MetalBuffer* metal_dst = static_cast<MetalBuffer*>(&dst);
        MetalBuffer const* metal_src = static_cast<MetalBuffer const*>(&src);

        if (!blit_encoder)
        {
            blit_encoder = [command_buffer blitCommandEncoder];
        }

        [blit_encoder copyFromBuffer:metal_src->GetMetalBuffer()
                        sourceOffset:0
                        toBuffer:metal_dst->GetMetalBuffer()
                        destinationOffset:0
                        size:src.GetSize()];
    }

    void MetalCommandList::CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size)
    {
        MetalBuffer* metal_dst = static_cast<MetalBuffer*>(&dst);
        MetalBuffer const* metal_src = static_cast<MetalBuffer const*>(&src);

        if (!blit_encoder)
        {
            blit_encoder = [command_buffer blitCommandEncoder];
        }

        [blit_encoder copyFromBuffer:metal_src->GetMetalBuffer()
                        sourceOffset:src_offset
                        toBuffer:metal_dst->GetMetalBuffer()
                        destinationOffset:dst_offset
                        size:size];
    }

    void MetalCommandList::BeginRenderPass(GfxRenderPassDesc const& render_pass_desc)
    {
        ADRIA_TODO("Fill this later");
        MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor new];
        render_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];

        MetalArgumentBuffer* arg_buffer = metal_device->GetArgumentBuffer();
        if (arg_buffer && render_encoder)
        {
            id<MTLBuffer> buffer = arg_buffer->GetBuffer();
            [render_encoder setVertexBuffer:buffer offset:0 atIndex:BINDLESS_ARGUMENT_BUFFER_SLOT];
            [render_encoder setFragmentBuffer:buffer offset:0 atIndex:BINDLESS_ARGUMENT_BUFFER_SLOT];
        }
    }

    void MetalCommandList::EndRenderPass()
    {
        if (render_encoder)
        {
            [render_encoder endEncoding];
            render_encoder = nil;
        }
    }

    void MetalCommandList::SetPipelineState(GfxPipelineState const* state)
    {
        current_pipeline_state = state;

        if (render_encoder && state)
        {
            if (state->GetType() == GfxPipelineStateType::Graphics)
            {
                MetalGraphicsPipelineState const* metal_pso = static_cast<MetalGraphicsPipelineState const*>(state);
                [render_encoder setRenderPipelineState:metal_pso->GetPipelineState()];

                if (metal_pso->GetDepthStencilState())
                {
                    [render_encoder setDepthStencilState:metal_pso->GetDepthStencilState()];
                }

                [render_encoder setCullMode:ConvertCullMode(metal_pso->GetCullMode())];
                [render_encoder setFrontFacingWinding:metal_pso->GetFrontCounterClockwise()
                    ? MTLWindingCounterClockwise : MTLWindingClockwise];

                if (metal_pso->GetDepthBias() != 0.0f || metal_pso->GetSlopeScaledDepthBias() != 0.0f)
                {
                    [render_encoder setDepthBias:metal_pso->GetDepthBias()
                                       slopeScale:metal_pso->GetSlopeScaledDepthBias()
                                            clamp:metal_pso->GetDepthBiasClamp()];
                }
            }
            else if (state->GetType() == GfxPipelineStateType::MeshShader)
            {
                MetalMeshShadingPipelineState const* metal_pso = static_cast<MetalMeshShadingPipelineState const*>(state);
                [render_encoder setRenderPipelineState:metal_pso->GetPipelineState()];

                if (metal_pso->GetDepthStencilState())
                {
                    [render_encoder setDepthStencilState:metal_pso->GetDepthStencilState()];
                }

                [render_encoder setCullMode:ConvertCullMode(metal_pso->GetCullMode())];
                [render_encoder setFrontFacingWinding:metal_pso->GetFrontCounterClockwise()
                    ? MTLWindingCounterClockwise : MTLWindingClockwise];

                if (metal_pso->GetDepthBias() != 0.0f || metal_pso->GetSlopeScaledDepthBias() != 0.0f)
                {
                    [render_encoder setDepthBias:metal_pso->GetDepthBias()
                                       slopeScale:metal_pso->GetSlopeScaledDepthBias()
                                            clamp:metal_pso->GetDepthBiasClamp()];
                }
            }
        }
        else if (state && state->GetType() == GfxPipelineStateType::Compute)
        {
            if (!compute_encoder)
            {
                if (render_encoder)
                {
                    [render_encoder endEncoding];
                    render_encoder = nil;
                }
                if (blit_encoder)
                {
                    [blit_encoder endEncoding];
                    blit_encoder = nil;
                }

                compute_encoder = [command_buffer computeCommandEncoder];

                MetalArgumentBuffer* arg_buffer = metal_device->GetArgumentBuffer();
                if (arg_buffer && compute_encoder)
                {
                    id<MTLBuffer> buffer = arg_buffer->GetBuffer();
                    [compute_encoder setBuffer:buffer offset:0 atIndex:BINDLESS_ARGUMENT_BUFFER_SLOT];
                }
            }

            MetalComputePipelineState const* metal_pso = static_cast<MetalComputePipelineState const*>(state);
            [compute_encoder setComputePipelineState:metal_pso->GetPipelineState()];
        }
    }

    void MetalCommandList::SetStencilReference(Uint8 stencil)
    {
        if (render_encoder)
        {
            [render_encoder setStencilReferenceValue:stencil];
        }
    }

    void MetalCommandList::SetPrimitiveTopology(GfxPrimitiveTopology topology)
    {
        current_topology = topology;
    }

    void MetalCommandList::SetIndexBuffer(GfxIndexBufferView* index_buffer_view)
    {
        current_index_buffer_view = index_buffer_view;
    }

    void MetalCommandList::SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot)
    {
        if (render_encoder)
        {
            MetalDevice::BufferLookupResult lookup = metal_device->LookupBuffer(vertex_buffer_view.buffer_location);
            if (lookup.buffer != nil)
            {
                [render_encoder setVertexBuffer:lookup.buffer
                                        offset:lookup.offset
                                        atIndex:start_slot];
            }
        }
    }

    void MetalCommandList::SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot)
    {
        for (Uint32 i = 0; i < vertex_buffer_views.size(); ++i)
        {
            SetVertexBuffer(vertex_buffer_views[i], start_slot + i);
        }
    }

    void MetalCommandList::SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
    {
        if (render_encoder)
        {
            MTLViewport viewport;
            viewport.originX = x;
            viewport.originY = y;
            viewport.width = width;
            viewport.height = height;
            viewport.znear = 0.0;
            viewport.zfar = 1.0;
            [render_encoder setViewport:viewport];
        }
    }

    void MetalCommandList::SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
    {
        if (render_encoder)
        {
            MTLScissorRect scissor;
            scissor.x = x;
            scissor.y = y;
            scissor.width = width;
            scissor.height = height;
            [render_encoder setScissorRect:scissor];
        }
    }

    GfxRayTracingShaderBindings* MetalCommandList::BeginRayTracingShaderBindings(GfxRayTracingPipeline const* pipeline)
    {
        ADRIA_ASSERT(pipeline != nullptr);
        ADRIA_ASSERT(pipeline->IsValid());

        MetalRayTracingPipeline const* metal_pipeline = static_cast<MetalRayTracingPipeline const*>(pipeline);

        if (render_encoder)
        {
            [render_encoder endEncoding];
            render_encoder = nil;
        }
        if (blit_encoder)
        {
            [blit_encoder endEncoding];
            blit_encoder = nil;
        }

        if (!compute_encoder)
        {
            compute_encoder = [command_buffer computeCommandEncoder];

            MetalArgumentBuffer* arg_buffer = metal_device->GetArgumentBuffer();
            if (arg_buffer && compute_encoder)
            {
                id<MTLBuffer> buffer = arg_buffer->GetBuffer();
                [compute_encoder setBuffer:buffer offset:0 atIndex:BINDLESS_ARGUMENT_BUFFER_SLOT];
            }
        }

        id<MTLComputePipelineState> raygen_pipeline = metal_pipeline->GetRayGenPipeline();
        [compute_encoder setComputePipelineState:raygen_pipeline];

        id<MTLIntersectionFunctionTable> intersection_table = metal_pipeline->GetIntersectionTable();
        if (intersection_table)
        {
            [compute_encoder setIntersectionFunctionTable:intersection_table atBufferIndex:0];
        }

        current_rt_bindings = std::make_unique<MetalRayTracingShaderBindings>(metal_pipeline);
        return current_rt_bindings.get();
    }

    void MetalCommandList::DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth)
    {
        if (dispatch_width == 0 || dispatch_height == 0 || dispatch_depth == 0)
        {
            return;
        }

        ADRIA_ASSERT(current_rt_bindings != nullptr);
        ADRIA_ASSERT(compute_encoder != nullptr);
        current_rt_bindings->Commit();

        MTLSize threadgroupsPerGrid = MTLSizeMake(
            (dispatch_width + 7) / 8,
            (dispatch_height + 7) / 8,
            dispatch_depth
        );
        MTLSize threadsPerThreadgroup = MTLSizeMake(8, 8, 1);

        [compute_encoder dispatchThreadgroups:threadgroupsPerGrid
                        threadsPerThreadgroup:threadsPerThreadgroup];

        current_rt_bindings.reset();
    }
}
