#import <Metal/Metal.h>
#define IR_PRIVATE_IMPLEMENTATION
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "MetalCommandList.h"
#include "MetalDevice.h"
#include "MetalBuffer.h"
#include "MetalTexture.h"
#include "MetalPipelineState.h"
#include "MetalConversions.h"
#include "MetalRayTracingPipeline.h"
#include "MetalRayTracingShaderBindings.h"
#include "MetalDescriptor.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxFence.h"
#include "Graphics/GfxFormat.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxDynamicAllocation.h"


namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

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
          compute_encoder(nil), blit_encoder(nil), encoder_fence(nil),
          current_topology(GfxPrimitiveTopology::Undefined),
          current_pipeline_state(nullptr), current_index_buffer_view(nullptr)
    {
        std::memset(&top_level_ab, 0, sizeof(TopLevelArgumentBuffer));
        top_level_ab.sampler_table_address = metal_device->GetSamplerTableGpuAddress();

        id<MTLDevice> device = metal_device->GetMTLDevice();
        encoder_fence = [device newFence];
    }

    MetalCommandList::~MetalCommandList()
    {
        @autoreleasepool
        {
            render_encoder = nil;
            compute_encoder = nil;
            blit_encoder = nil;
            encoder_fence = nil;
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
        EndBlitEncoder();
        EndComputeEncoder();
        EndRenderPass();
    }

    void MetalCommandList::Submit()
    {
        if (command_buffer)
        {
            SignalAll();
            [command_buffer commit];
        }
        else
        {
            ADRIA_LOG(ERROR, "MetalCommandList::Submit() - command_buffer is nil!");
        }
    }

    void MetalCommandList::Wait(GfxFence& fence, Uint64 value)
    {
        if (command_buffer)
        {
            id<MTLSharedEvent> shared_event = (__bridge id<MTLSharedEvent>)fence.GetHandle();
            if (shared_event)
            {
                [command_buffer encodeWaitForEvent:shared_event value:value];
            }
        }
    }

    void MetalCommandList::Signal(GfxFence& fence, Uint64 value)
    {
        pending_signals.emplace_back(fence, value);
    }

    void MetalCommandList::SignalAll()
    {
        for (Uint64 i = 0; i < pending_signals.size(); ++i)
        {
            id<MTLSharedEvent> shared_event = (__bridge id<MTLSharedEvent>)pending_signals[i].first.GetHandle();
            if (shared_event && command_buffer)
            {
                [command_buffer encodeSignalEvent:shared_event value:pending_signals[i].second];
            }
        }
        pending_signals.clear();
    }

    void MetalCommandList::ResetState()
    {
        current_topology = GfxPrimitiveTopology::Undefined;
        current_pipeline_state = nullptr;
        current_index_buffer_view = nullptr;
        current_stencil_ref = 0;

        cached_cull_mode = MTLCullModeNone;
        cached_front_face_winding = MTLWindingClockwise;
        cached_depth_bias = 0.0f;
        cached_depth_slope_scale = 0.0f;
        cached_depth_bias_clamp = 0.0f;
        cached_depth_stencil_state = nil;
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
            [render_encoder setVertexBytes:&top_level_ab
                                    length:sizeof(TopLevelArgumentBuffer)
                                   atIndex:kIRArgumentBufferBindPoint];
            [render_encoder setFragmentBytes:&top_level_ab
                                      length:sizeof(TopLevelArgumentBuffer)
                                     atIndex:kIRArgumentBufferBindPoint];

            IRRuntimeDrawPrimitives(render_encoder, ConvertTopology(current_topology), start_vertex_location, vertex_count, instance_count, start_instance_location);
        }
    }

    void MetalCommandList::DrawIndexed(Uint32 index_count, Uint32 instance_count, Uint32 index_offset, Uint32 base_vertex_location, Uint32 start_instance_location)
    {
        if (render_encoder && current_index_buffer_view)
        {
            [render_encoder setVertexBytes:&top_level_ab
                                    length:sizeof(TopLevelArgumentBuffer)
                                   atIndex:kIRArgumentBufferBindPoint];
            [render_encoder setFragmentBytes:&top_level_ab
                                      length:sizeof(TopLevelArgumentBuffer)
                                     atIndex:kIRArgumentBufferBindPoint];

            MetalDevice::BufferLookupResult lookup = metal_device->LookupBuffer(current_index_buffer_view->buffer_location);
            if (lookup.buffer != nil)
            {
                MTLIndexType index_type = ConvertIndexFormat(current_index_buffer_view->format);
                Uint32 index_size = (index_type == MTLIndexTypeUInt16) ? 2 : 4;
                Uint64 index_buffer_offset = lookup.offset + (index_offset * index_size);

                IRRuntimeDrawIndexedPrimitives(render_encoder, ConvertTopology(current_topology), index_count, index_type, lookup.buffer, index_buffer_offset, instance_count, base_vertex_location, start_instance_location);
            }
        }
    }

    void MetalCommandList::Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
    {
        if (compute_encoder && current_pipeline_state && current_pipeline_state->GetType() == GfxPipelineStateType::Compute)
        {
            [compute_encoder setBytes:&top_level_ab
                               length:sizeof(TopLevelArgumentBuffer)
                              atIndex:kIRArgumentBufferBindPoint];

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
            [render_encoder setObjectBytes:&top_level_ab
                                    length:sizeof(TopLevelArgumentBuffer)
                                   atIndex:kIRArgumentBufferBindPoint];
            [render_encoder setMeshBytes:&top_level_ab
                                  length:sizeof(TopLevelArgumentBuffer)
                                 atIndex:kIRArgumentBufferBindPoint];
            [render_encoder setFragmentBytes:&top_level_ab
                                      length:sizeof(TopLevelArgumentBuffer)
                                     atIndex:kIRArgumentBufferBindPoint];

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

        BeginBlitEncoder();
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

        BeginBlitEncoder();
        [blit_encoder copyFromBuffer:metal_src->GetMetalBuffer()
                        sourceOffset:src_offset
                        toBuffer:metal_dst->GetMetalBuffer()
                        destinationOffset:dst_offset
                        size:size];
    }

    void MetalCommandList::CopyTexture(GfxTexture& dst, GfxTexture const& src)
    {
        MetalTexture* metal_dst = static_cast<MetalTexture*>(&dst);
        MetalTexture const* metal_src = static_cast<MetalTexture const*>(&src);

        Uint32 src_width = std::max(1u, src.GetWidth());
        Uint32 src_height = std::max(1u, src.GetHeight());
        Uint32 src_depth = std::max(1u, src.GetDepth());

        BeginBlitEncoder();
        [blit_encoder copyFromTexture:metal_src->GetMetalTexture()
                          sourceSlice:0
                          sourceLevel:0
                         sourceOrigin:MTLOriginMake(0, 0, 0)
                           sourceSize:MTLSizeMake(src_width, src_height, src_depth)
                            toTexture:metal_dst->GetMetalTexture()
                     destinationSlice:0
                     destinationLevel:0
                    destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    void MetalCommandList::CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
    {
        MetalTexture* metal_dst = static_cast<MetalTexture*>(&dst);
        MetalTexture const* metal_src = static_cast<MetalTexture const*>(&src);

        Uint32 src_width = std::max(1u, src.GetWidth() >> src_mip);
        Uint32 src_height = std::max(1u, src.GetHeight() >> src_mip);
        Uint32 src_depth = std::max(1u, src.GetDepth() >> src_mip);

        BeginBlitEncoder();
        [blit_encoder copyFromTexture:metal_src->GetMetalTexture()
                          sourceSlice:src_array
                          sourceLevel:src_mip
                         sourceOrigin:MTLOriginMake(0, 0, 0)
                           sourceSize:MTLSizeMake(src_width, src_height, src_depth)
                            toTexture:metal_dst->GetMetalTexture()
                     destinationSlice:dst_array
                     destinationLevel:dst_mip
                    destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    void MetalCommandList::CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
    {
        MetalBuffer* metal_dst = static_cast<MetalBuffer*>(&dst);
        MetalTexture const* metal_src = static_cast<MetalTexture const*>(&src);

        Uint32 src_width = std::max(1u, src.GetWidth() >> src_mip);
        Uint32 src_height = std::max(1u, src.GetHeight() >> src_mip);
        Uint32 src_depth = std::max(1u, src.GetDepth() >> src_mip);

        Uint32 bytes_per_row = metal_src->GetRowPitch(src_mip);
        Uint32 block_size = GetGfxFormatBlockSize(src.GetFormat());
        Uint32 row_count = std::max(1u, DivideAndRoundUp(src_height, block_size));
        Uint32 bytes_per_image = bytes_per_row * row_count;

        BeginBlitEncoder();
        [blit_encoder copyFromTexture:metal_src->GetMetalTexture()
                          sourceSlice:src_array
                          sourceLevel:src_mip
                         sourceOrigin:MTLOriginMake(0, 0, 0)
                           sourceSize:MTLSizeMake(src_width, src_height, src_depth)
                             toBuffer:metal_dst->GetMetalBuffer()
                    destinationOffset:dst_offset
               destinationBytesPerRow:bytes_per_row
             destinationBytesPerImage:bytes_per_image];
    }

    void MetalCommandList::CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset)
    {
        MetalTexture* metal_dst = static_cast<MetalTexture*>(&dst_texture);
        MetalBuffer const* metal_src = static_cast<MetalBuffer const*>(&src_buffer);

        Uint32 dst_width = std::max(1u, dst_texture.GetWidth() >> mip_level);
        Uint32 dst_height = std::max(1u, dst_texture.GetHeight() >> mip_level);
        Uint32 dst_depth = std::max(1u, dst_texture.GetDepth() >> mip_level);

        Uint32 bytes_per_row = metal_dst->GetRowPitch(mip_level);
        Uint32 block_size = GetGfxFormatBlockSize(dst_texture.GetFormat());
        Uint32 row_count = std::max(1u, DivideAndRoundUp(dst_height, block_size));
        Uint32 bytes_per_image = bytes_per_row * row_count;

        BeginBlitEncoder();
        [blit_encoder copyFromBuffer:metal_src->GetMetalBuffer()
                        sourceOffset:offset
                   sourceBytesPerRow:bytes_per_row
                 sourceBytesPerImage:bytes_per_image
                          sourceSize:MTLSizeMake(dst_width, dst_height, dst_depth)
                           toTexture:metal_dst->GetMetalTexture()
                    destinationSlice:array_slice
                    destinationLevel:mip_level
                   destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    void MetalCommandList::ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color)
    {
        EndRenderPass();
        EndComputeEncoder();
        EndBlitEncoder();

        MetalRenderTargetDescriptor rtv_desc = DecodeToMetalRenderTargetDescriptor(rtv);
        if (!rtv_desc.texture)
        {
            return;
        }

        MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor new];
        MTLRenderPassColorAttachmentDescriptor* color_attachment = pass_desc.colorAttachments[0];

        color_attachment.texture = rtv_desc.texture;
        color_attachment.level = rtv_desc.mip_level;
        color_attachment.slice = rtv_desc.array_slice;
        color_attachment.clearColor = MTLClearColorMake(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
        color_attachment.loadAction = MTLLoadActionClear;
        color_attachment.storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> clear_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];
        [clear_encoder endEncoding];
    }

    void MetalCommandList::ClearDepth(GfxDescriptor dsv, Float depth, Uint8 stencil, Bool clear_stencil)
    {
        EndRenderPass();
        EndComputeEncoder();
        EndBlitEncoder();

        MetalRenderTargetDescriptor dsv_desc = DecodeToMetalRenderTargetDescriptor(dsv);
        if (!dsv_desc.texture)
        {
            return;
        }

        MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor new];

        MTLRenderPassDepthAttachmentDescriptor* depth_attachment = pass_desc.depthAttachment;
        depth_attachment.texture = dsv_desc.texture;
        depth_attachment.level = dsv_desc.mip_level;
        depth_attachment.slice = dsv_desc.array_slice;
        depth_attachment.clearDepth = depth;
        depth_attachment.loadAction = MTLLoadActionClear;
        depth_attachment.storeAction = MTLStoreActionStore;

        if (clear_stencil)
        {
            MTLRenderPassStencilAttachmentDescriptor* stencil_attachment = pass_desc.stencilAttachment;
            stencil_attachment.texture = dsv_desc.texture;
            stencil_attachment.level = dsv_desc.mip_level;
            stencil_attachment.slice = dsv_desc.array_slice;
            stencil_attachment.clearStencil = stencil;
            stencil_attachment.loadAction = MTLLoadActionClear;
            stencil_attachment.storeAction = MTLStoreActionStore;
        }

        id<MTLRenderCommandEncoder> clear_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];
        [clear_encoder endEncoding];
    }

    void MetalCommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Float const clear_value[4])
    {
        MetalBuffer const* metal_buffer = static_cast<MetalBuffer const*>(&resource);
        Uint64 size = (uav_desc.size == 0 || uav_desc.size == Uint64(-1)) ? resource.GetSize() : uav_desc.size;
        if (clear_value[0] == 0.0f && clear_value[1] == 0.0f && clear_value[2] == 0.0f && clear_value[3] == 0.0f)
        {
            BeginBlitEncoder();
            [blit_encoder fillBuffer:metal_buffer->GetMetalBuffer()
                               range:NSMakeRange(uav_desc.offset, size)
                               value:0];
        }
        else
        {
            BeginComputeEncoder();

            id<MTLComputePipelineState> pipeline = metal_device->GetClearPipeline(MetalClearPipeline::ClearBufferFloat4);
            [compute_encoder setComputePipelineState:pipeline];

            struct ClearBufferParams
            {
                Uint32 clear_value[4];
                Uint32 element_count;
            } params;

            std::memcpy(params.clear_value, clear_value, sizeof(Float) * 4);
            params.element_count = static_cast<Uint32>(size / (sizeof(Float) * 4));

            [compute_encoder setBuffer:metal_buffer->GetMetalBuffer() offset:uav_desc.offset atIndex:0];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:1];

            Uint32 thread_group_size = 256;
            Uint32 thread_groups = (params.element_count + thread_group_size - 1) / thread_group_size;
            [compute_encoder dispatchThreadgroups:MTLSizeMake(thread_groups, 1, 1)
                            threadsPerThreadgroup:MTLSizeMake(thread_group_size, 1, 1)];
        }
    }

    void MetalCommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Float const clear_value[4])
    {
        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(&resource);
        GfxTextureDesc const& desc = resource.GetDesc();

        Uint32 first_mip = uav_desc.first_mip;
        Uint32 first_slice = uav_desc.first_slice;
        Uint32 mip_width = std::max(1u, desc.width >> first_mip);
        Uint32 mip_height = std::max(1u, desc.height >> first_mip);
        Uint32 mip_depth = std::max(1u, desc.depth >> first_mip);
        Uint32 slice_count = (uav_desc.slice_count == static_cast<Uint32>(-1)) ? (desc.array_size - first_slice) : uav_desc.slice_count;

        BeginComputeEncoder();

        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();
        id<MTLTexture> texture_view = nil;

        Bool is_array = (desc.array_size > 1) && (desc.type != GfxTextureType_3D);

        if (is_array)
        {
            texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                           textureType:base_texture.textureType
                                                                levels:NSMakeRange(first_mip, 1)
                                                                slices:NSMakeRange(first_slice, slice_count)];
        }
        else if (first_mip != 0)
        {
            texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                           textureType:base_texture.textureType
                                                                levels:NSMakeRange(first_mip, 1)
                                                                slices:NSMakeRange(0, 1)];
        }
        else
        {
            texture_view = base_texture;
        }

        struct ClearTextureParams
        {
            Uint32 clear_value_uint[4];
            Float  clear_value_float[4];
            Uint32 dimensions[3];
            Uint32 _padding;
        } params;

        std::memcpy(params.clear_value_uint, clear_value, sizeof(Float) * 4);
        std::memcpy(params.clear_value_float, clear_value, sizeof(Float) * 4);

        MetalClearPipeline pipeline_type;
        MTLSize threadgroups;
        MTLSize threads_per_group;

        switch (desc.type)
        {
        case GfxTextureType_1D:
            if (is_array)
            {
                pipeline_type = MetalClearPipeline::ClearTexture1DArrayFloat;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = slice_count;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 255) / 256, slice_count, 1);
                threads_per_group = MTLSizeMake(256, 1, 1);
            }
            else
            {
                pipeline_type = MetalClearPipeline::ClearTexture1DFloat;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = 1;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 255) / 256, 1, 1);
                threads_per_group = MTLSizeMake(256, 1, 1);
            }
            break;
        case GfxTextureType_2D:
            if (is_array)
            {
                pipeline_type = MetalClearPipeline::ClearTexture2DArrayFloat;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = mip_height;
                params.dimensions[2] = slice_count;
                threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, slice_count);
                threads_per_group = MTLSizeMake(8, 8, 1);
            }
            else
            {
                pipeline_type = MetalClearPipeline::ClearTexture2DFloat;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = mip_height;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, 1);
                threads_per_group = MTLSizeMake(8, 8, 1);
            }
            break;
        case GfxTextureType_3D:
            pipeline_type = MetalClearPipeline::ClearTexture3DFloat;
            params.dimensions[0] = mip_width;
            params.dimensions[1] = mip_height;
            params.dimensions[2] = mip_depth;
            threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, (mip_depth + 3) / 4);
            threads_per_group = MTLSizeMake(8, 8, 4);
            break;
        default:
            ADRIA_LOG(WARNING, "Unknown texture type for clear");
            return;
        }

        id<MTLComputePipelineState> pipeline = metal_device->GetClearPipeline(pipeline_type);
        [compute_encoder setComputePipelineState:pipeline];
        [compute_encoder setTexture:texture_view atIndex:10];
        [compute_encoder setBytes:&params length:sizeof(params) atIndex:10];
        [compute_encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threads_per_group];
    }

    void MetalCommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
    {
        MetalBuffer const* metal_buffer = static_cast<MetalBuffer const*>(&resource);
        Uint64 size = (uav_desc.size == 0 || uav_desc.size == Uint64(-1)) ? resource.GetSize() : uav_desc.size;

        if (clear_value[0] == 0 && clear_value[1] == 0 && clear_value[2] == 0 && clear_value[3] == 0)
        {
            BeginBlitEncoder();
            [blit_encoder fillBuffer:metal_buffer->GetMetalBuffer()
                               range:NSMakeRange(uav_desc.offset, size)
                               value:0];
        }
        else
        {
            BeginComputeEncoder();

            id<MTLComputePipelineState> pipeline = metal_device->GetClearPipeline(MetalClearPipeline::ClearBufferUint4);
            [compute_encoder setComputePipelineState:pipeline];

            struct ClearBufferParams
            {
                Uint32 clear_value[4];
                Uint32 element_count;
            } params;

            std::memcpy(params.clear_value, clear_value, sizeof(Uint32) * 4);
            params.element_count = static_cast<Uint32>(size / (sizeof(Uint32) * 4));

            [compute_encoder setBuffer:metal_buffer->GetMetalBuffer() offset:uav_desc.offset atIndex:0];
            [compute_encoder setBytes:&params length:sizeof(params) atIndex:1];

            Uint32 thread_group_size = 256;
            Uint32 thread_groups = (params.element_count + thread_group_size - 1) / thread_group_size;
            [compute_encoder dispatchThreadgroups:MTLSizeMake(thread_groups, 1, 1)
                            threadsPerThreadgroup:MTLSizeMake(thread_group_size, 1, 1)];
        }
    }

    void MetalCommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
    {
        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(&resource);
        GfxTextureDesc const& desc = resource.GetDesc();

        Uint32 first_mip = uav_desc.first_mip;
        Uint32 first_slice = uav_desc.first_slice;
        Uint32 mip_width = std::max(1u, desc.width >> first_mip);
        Uint32 mip_height = std::max(1u, desc.height >> first_mip);
        Uint32 mip_depth = std::max(1u, desc.depth >> first_mip);
        Uint32 slice_count = (uav_desc.slice_count == static_cast<Uint32>(-1)) ? (desc.array_size - first_slice) : uav_desc.slice_count;

        BeginComputeEncoder();

        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();
        id<MTLTexture> texture_view = nil;

        Bool is_array = (desc.array_size > 1) && (desc.type != GfxTextureType_3D);

        if (is_array)
        {
            texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                           textureType:base_texture.textureType
                                                                levels:NSMakeRange(first_mip, 1)
                                                                slices:NSMakeRange(first_slice, slice_count)];
        }
        else if (first_mip != 0)
        {
            texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                           textureType:base_texture.textureType
                                                                levels:NSMakeRange(first_mip, 1)
                                                                slices:NSMakeRange(0, 1)];
        }
        else
        {
            texture_view = base_texture;
        }

        struct ClearTextureParams
        {
            Uint32 clear_value_uint[4];
            Float  clear_value_float[4];
            Uint32 dimensions[3];
            Uint32 _padding;
        } params;

        std::memcpy(params.clear_value_uint, clear_value, sizeof(Uint32) * 4);
        std::memcpy(params.clear_value_float, clear_value, sizeof(Uint32) * 4);

        MetalClearPipeline pipeline_type;
        MTLSize threadgroups;
        MTLSize threads_per_group;

        switch (desc.type)
        {
        case GfxTextureType_1D:
            if (is_array)
            {
                pipeline_type = MetalClearPipeline::ClearTexture1DArrayUint;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = slice_count;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 255) / 256, slice_count, 1);
                threads_per_group = MTLSizeMake(256, 1, 1);
            }
            else
            {
                pipeline_type = MetalClearPipeline::ClearTexture1DUint;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = 1;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 255) / 256, 1, 1);
                threads_per_group = MTLSizeMake(256, 1, 1);
            }
            break;
        case GfxTextureType_2D:
            if (is_array)
            {
                pipeline_type = MetalClearPipeline::ClearTexture2DArrayUint;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = mip_height;
                params.dimensions[2] = slice_count;
                threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, slice_count);
                threads_per_group = MTLSizeMake(8, 8, 1);
            }
            else
            {
                pipeline_type = MetalClearPipeline::ClearTexture2DUint;
                params.dimensions[0] = mip_width;
                params.dimensions[1] = mip_height;
                params.dimensions[2] = 1;
                threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, 1);
                threads_per_group = MTLSizeMake(8, 8, 1);
            }
            break;
        case GfxTextureType_3D:
            pipeline_type = MetalClearPipeline::ClearTexture3DUint;
            params.dimensions[0] = mip_width;
            params.dimensions[1] = mip_height;
            params.dimensions[2] = mip_depth;
            threadgroups = MTLSizeMake((mip_width + 7) / 8, (mip_height + 7) / 8, (mip_depth + 3) / 4);
            threads_per_group = MTLSizeMake(8, 8, 4);
            break;
        default:
            ADRIA_LOG(WARNING, "Unknown texture type for clear");
            return;
        }

        id<MTLComputePipelineState> pipeline = metal_device->GetClearPipeline(pipeline_type);
        [compute_encoder setComputePipelineState:pipeline];
        [compute_encoder setTexture:texture_view atIndex:10];
        [compute_encoder setBytes:&params length:sizeof(params) atIndex:10];
        [compute_encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threads_per_group];
    }

    void MetalCommandList::WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data)
    {
        MetalBuffer* metal_buffer = static_cast<MetalBuffer*>(&buffer);

        void* mapped_ptr = metal_buffer->Map();
        if (mapped_ptr)
        {
            *((Uint32*)((Uint8*)mapped_ptr + offset)) = data;
            metal_buffer->Unmap();
        }
        else
        {
            ADRIA_TODO();
            ADRIA_LOG(WARNING, "WriteBufferImmediate not implemented for non-mappable buffers");
        }
    }

    void MetalCommandList::BeginRenderPass(GfxRenderPassDesc const& render_pass_desc)
    {
        EndBlitEncoder();
        EndComputeEncoder();

        MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor new];
        for (Uint32 i = 0; i < render_pass_desc.rtv_attachments.size(); ++i)
        {
            GfxColorAttachmentDesc const& color_attachment = render_pass_desc.rtv_attachments[i];
            MetalRenderTargetDescriptor rtv_desc = DecodeToMetalRenderTargetDescriptor(color_attachment.cpu_handle);
            if (!rtv_desc.texture)
            {
                continue;
            }

            MTLRenderPassColorAttachmentDescriptor* mtl_color_attachment = pass_desc.colorAttachments[i];
            mtl_color_attachment.texture = rtv_desc.texture;
            mtl_color_attachment.level = rtv_desc.mip_level;
            mtl_color_attachment.slice = rtv_desc.array_slice;
            mtl_color_attachment.loadAction = ConvertLoadAction(color_attachment.beginning_access);
            mtl_color_attachment.storeAction = ConvertStoreAction(color_attachment.ending_access);

            if (color_attachment.beginning_access == GfxLoadAccessOp::Clear)
            {
                if (color_attachment.clear_value.active_member == GfxClearValue::GfxActiveMember::Color)
                {
                    Float const* clear_color = color_attachment.clear_value.color.color;
                    mtl_color_attachment.clearColor = MTLClearColorMake(
                        clear_color[0], clear_color[1], clear_color[2], clear_color[3]
                    );
                }
            }
        }

        if (render_pass_desc.dsv_attachment.has_value())
        {
            GfxDepthAttachmentDesc const& depth_attachment = render_pass_desc.dsv_attachment.value();
            MetalRenderTargetDescriptor dsv_desc = DecodeToMetalRenderTargetDescriptor(depth_attachment.cpu_handle);
            if (dsv_desc.texture)
            {
                MTLRenderPassDepthAttachmentDescriptor* mtl_depth_attachment = pass_desc.depthAttachment;
                mtl_depth_attachment.texture = dsv_desc.texture;
                mtl_depth_attachment.level = dsv_desc.mip_level;
                mtl_depth_attachment.slice = dsv_desc.array_slice;
                mtl_depth_attachment.loadAction = ConvertLoadAction(depth_attachment.depth_beginning_access);
                mtl_depth_attachment.storeAction = ConvertStoreAction(depth_attachment.depth_ending_access);

                if (depth_attachment.depth_beginning_access == GfxLoadAccessOp::Clear)
                {
                    if (depth_attachment.clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil)
                    {
                        mtl_depth_attachment.clearDepth = depth_attachment.clear_value.depth_stencil.depth;
                    }
                }

                if (depth_attachment.stencil_beginning_access != GfxLoadAccessOp::NoAccess)
                {
                    MTLRenderPassStencilAttachmentDescriptor* mtl_stencil_attachment = pass_desc.stencilAttachment;
                    mtl_stencil_attachment.texture = dsv_desc.texture;
                    mtl_stencil_attachment.level = dsv_desc.mip_level;
                    mtl_stencil_attachment.slice = dsv_desc.array_slice;
                    mtl_stencil_attachment.loadAction = ConvertLoadAction(depth_attachment.stencil_beginning_access);
                    mtl_stencil_attachment.storeAction = ConvertStoreAction(depth_attachment.stencil_ending_access);
                    if (depth_attachment.stencil_beginning_access == GfxLoadAccessOp::Clear)
                    {
                        if (depth_attachment.clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil)
                        {
                            mtl_stencil_attachment.clearStencil = depth_attachment.clear_value.depth_stencil.stencil;
                        }
                    }
                }
            }
        }

        render_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];
        [render_encoder waitForFence:encoder_fence beforeStages:MTLRenderStageVertex | MTLRenderStageObject];

        id<MTLBuffer> descriptor_buffer = metal_device->GetResourceDescriptorBuffer();
        if (descriptor_buffer && render_encoder)
        {
            [render_encoder setVertexBuffer:descriptor_buffer offset:0 atIndex:kIRDescriptorHeapBindPoint];
            [render_encoder setFragmentBuffer:descriptor_buffer offset:0 atIndex:kIRDescriptorHeapBindPoint];

            [render_encoder setObjectBuffer:descriptor_buffer offset:0 atIndex:kIRDescriptorHeapBindPoint];
            [render_encoder setMeshBuffer:descriptor_buffer offset:0 atIndex:kIRDescriptorHeapBindPoint];
        }
        SetViewport(0, 0, render_pass_desc.width, render_pass_desc.height);
        SetScissorRect(0, 0, render_pass_desc.width, render_pass_desc.height);
    }

    void MetalCommandList::EndRenderPass()
    {
        if (render_encoder)
        {
            [render_encoder updateFence:encoder_fence afterStages:MTLRenderStageFragment];
            [render_encoder endEncoding];
            render_encoder = nil;
            ResetState();
        }
    }

    void MetalCommandList::SetPipelineState(GfxPipelineState const* state)
    {
        if (state == current_pipeline_state)
        {
            return;
        }
        current_pipeline_state = state;

        if (render_encoder && state)
        {
            if (state->GetType() == GfxPipelineStateType::Graphics)
            {
                MetalGraphicsPipelineState const* metal_pso = static_cast<MetalGraphicsPipelineState const*>(state);
                id<MTLRenderPipelineState> pipeline = metal_pso->GetPipelineState();
                [render_encoder setRenderPipelineState:pipeline];

                id<MTLDepthStencilState> depth_stencil = metal_pso->GetDepthStencilState();
                if (depth_stencil && depth_stencil != cached_depth_stencil_state)
                {
                    [render_encoder setDepthStencilState:depth_stencil];
                    cached_depth_stencil_state = depth_stencil;
                }

                MTLCullMode cull_mode = ConvertCullMode(metal_pso->GetCullMode());
                if (cull_mode != cached_cull_mode)
                {
                    [render_encoder setCullMode:cull_mode];
                    cached_cull_mode = cull_mode;
                }

                MTLWinding winding = metal_pso->GetFrontCounterClockwise() ? MTLWindingCounterClockwise : MTLWindingClockwise;
                if (winding != cached_front_face_winding)
                {
                    [render_encoder setFrontFacingWinding:winding];
                    cached_front_face_winding = winding;
                }

                Float depth_bias = metal_pso->GetDepthBias();
                Float slope_scale = metal_pso->GetSlopeScaledDepthBias();
                Float bias_clamp = metal_pso->GetDepthBiasClamp();
                if (depth_bias != cached_depth_bias || slope_scale != cached_depth_slope_scale || bias_clamp != cached_depth_bias_clamp)
                {
                    [render_encoder setDepthBias:depth_bias slopeScale:slope_scale clamp:bias_clamp];
                    cached_depth_bias = depth_bias;
                    cached_depth_slope_scale = slope_scale;
                    cached_depth_bias_clamp = bias_clamp;
                }
            }
            else if (state->GetType() == GfxPipelineStateType::MeshShader)
            {
                MetalMeshShadingPipelineState const* metal_pso = static_cast<MetalMeshShadingPipelineState const*>(state);
                [render_encoder setRenderPipelineState:metal_pso->GetPipelineState()];

                id<MTLDepthStencilState> depth_stencil = metal_pso->GetDepthStencilState();
                if (depth_stencil && depth_stencil != cached_depth_stencil_state)
                {
                    [render_encoder setDepthStencilState:depth_stencil];
                    cached_depth_stencil_state = depth_stencil;
                }

                MTLCullMode cull_mode = ConvertCullMode(metal_pso->GetCullMode());
                if (cull_mode != cached_cull_mode)
                {
                    [render_encoder setCullMode:cull_mode];
                    cached_cull_mode = cull_mode;
                }

                MTLWinding winding = metal_pso->GetFrontCounterClockwise() ? MTLWindingCounterClockwise : MTLWindingClockwise;
                if (winding != cached_front_face_winding)
                {
                    [render_encoder setFrontFacingWinding:winding];
                    cached_front_face_winding = winding;
                }

                Float depth_bias = metal_pso->GetDepthBias();
                Float slope_scale = metal_pso->GetSlopeScaledDepthBias();
                Float bias_clamp = metal_pso->GetDepthBiasClamp();
                if (depth_bias != cached_depth_bias || slope_scale != cached_depth_slope_scale || bias_clamp != cached_depth_bias_clamp)
                {
                    [render_encoder setDepthBias:depth_bias slopeScale:slope_scale clamp:bias_clamp];
                    cached_depth_bias = depth_bias;
                    cached_depth_slope_scale = slope_scale;
                    cached_depth_bias_clamp = bias_clamp;
                }
            }
        }
        else if (state && state->GetType() == GfxPipelineStateType::Compute)
        {
            BeginComputeEncoder();

            MetalComputePipelineState const* metal_pso = static_cast<MetalComputePipelineState const*>(state);
            [compute_encoder setComputePipelineState:metal_pso->GetPipelineState()];
        }
    }

    void MetalCommandList::SetStencilReference(Uint8 stencil)
    {
        if (stencil != current_stencil_ref)
        {
            current_stencil_ref = stencil;
            if (render_encoder)
            {
                [render_encoder setStencilReferenceValue:stencil];
            }
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
                Uint32 bind_index = kIRVertexBufferBindPoint + start_slot;
                [render_encoder setVertexBuffer:lookup.buffer
                                        offset:lookup.offset
                                        atIndex:bind_index];
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

    void MetalCommandList::SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset)
    {
        if (slot == 1 && offset < 8)  
        {
            top_level_ab.root_constants[offset] = data;
            top_level_ab_dirty = true;
        }
    }

    void MetalCommandList::SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset)
    {
        if (slot == 1 && data && data_size > 0)  
        {
            Uint32 num_constants = data_size / sizeof(Uint32);
            if (offset + num_constants <= 8)  
            {
                memcpy(&top_level_ab.root_constants[offset], data, data_size);
                top_level_ab_dirty = true;
            }
        }
    }

    void MetalCommandList::SetRootCBV(Uint32 slot, void const* data, Uint64 data_size)
    {
        GfxLinearDynamicAllocator* dynamic_allocator = metal_device->GetDynamicAllocator();
        GfxDynamicAllocation alloc = dynamic_allocator->Allocate(data_size, GFX_CONSTANT_BUFFER_DATA_ALIGNMENT);
        alloc.Update(data, data_size);

        if (slot == 0)
        {
            top_level_ab.cbv0_address = alloc.gpu_address;
            top_level_ab_dirty = true;
        }
        else if (slot == 2)
        {
            top_level_ab.cbv2_address = alloc.gpu_address;
            top_level_ab_dirty = true;
        }
        else if (slot == 3)
        {
            top_level_ab.cbv3_address = alloc.gpu_address;
            top_level_ab_dirty = true;
        }
    }

    void MetalCommandList::SetRootCBV(Uint32 slot, Uint64 gpu_address)
    {
        if (slot == 0)
        {
            top_level_ab.cbv0_address = gpu_address;
            top_level_ab_dirty = true;
        }
        else if (slot == 2)
        {
            top_level_ab.cbv2_address = gpu_address;
            top_level_ab_dirty = true;
        }
        else if (slot == 3)
        {
            top_level_ab.cbv3_address = gpu_address;
            top_level_ab_dirty = true;
        }
    }

    GfxDynamicAllocation MetalCommandList::AllocateTransient(Uint32 size, Uint32 align)
    {
        return metal_device->GetDynamicAllocator()->Allocate(size, align);
    }

    void MetalCommandList::SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor)
    {
        // Not needed for bindless - descriptors are accessed directly via index
    }

    void MetalCommandList::UpdateTopLevelArgumentBuffer()
    {
        top_level_ab_dirty = false;
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

        BeginComputeEncoder();

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

        [compute_encoder setBytes:&top_level_ab
                           length:sizeof(TopLevelArgumentBuffer)
                          atIndex:kIRArgumentBufferBindPoint];

        IRDispatchRaysArgument rt_args = {};

        rt_args.DispatchRaysDesc.Width = dispatch_width;
        rt_args.DispatchRaysDesc.Height = dispatch_height;
        rt_args.DispatchRaysDesc.Depth = dispatch_depth;

        // TODO: Fill in shader tables when ray tracing is fully implemented
        // rt_args.DispatchRaysDesc.RayGenerationShaderRecord = ...
        // rt_args.DispatchRaysDesc.MissShaderTable = ...
        // rt_args.DispatchRaysDesc.HitGroupTable = ...
        // rt_args.DispatchRaysDesc.CallableShaderTable = ...
        // rt_args.GRS = ...
        // rt_args.ResDescHeap = ...
        // rt_args.SmpDescHeap = ...
        // rt_args.VisibleFunctionTable = ...
        // rt_args.IntersectionFunctionTable = ...
        // rt_args.IntersectionFunctionTables = ...
        // Bind the ray tracing dispatch argument at index 3 for Metal Shader Converter
        [compute_encoder setBytes:&rt_args length:sizeof(IRDispatchRaysArgument) atIndex:3];

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

    void MetalCommandList::BeginBlitEncoder()
    {
        EndRenderPass();
        EndComputeEncoder();

        if (!blit_encoder)
        {
            blit_encoder = [command_buffer blitCommandEncoder];
            [blit_encoder waitForFence:encoder_fence];
        }
    }

    void MetalCommandList::EndBlitEncoder()
    {
        if (blit_encoder)
        {
            [blit_encoder updateFence:encoder_fence];
            [blit_encoder endEncoding];
            blit_encoder = nil;
        }
    }

    void MetalCommandList::BeginComputeEncoder()
    {
        if (render_encoder)
        {
            [render_encoder updateFence:encoder_fence afterStages:MTLRenderStageFragment];
            [render_encoder endEncoding];
            render_encoder = nil;
        }
        EndBlitEncoder();

        if (!compute_encoder)
        {
            compute_encoder = [command_buffer computeCommandEncoder];
            [compute_encoder waitForFence:encoder_fence];

            id<MTLBuffer> descriptor_buffer = metal_device->GetResourceDescriptorBuffer();
            if (descriptor_buffer && compute_encoder)
            {
                [compute_encoder setBuffer:descriptor_buffer offset:0 atIndex:kIRDescriptorHeapBindPoint];
            }
        }
    }

    void MetalCommandList::EndComputeEncoder()
    {
        if (compute_encoder)
        {
            [compute_encoder updateFence:encoder_fence];
            [compute_encoder endEncoding];
            compute_encoder = nil;
        }
    }
}
