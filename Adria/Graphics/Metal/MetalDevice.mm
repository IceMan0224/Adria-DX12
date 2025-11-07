#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalDevice.h"
#include "MetalSwapchain.h"
#include "MetalTexture.h"
#include "MetalBuffer.h"
#include "MetalCommandList.h"
#include "MetalPipelineState.h"
#include "MetalArgumentBuffer.h"
#include "MetalDescriptor.h"
#include "MetalConversions.h"
#include "MetalRayTracingAS.h"
#include "MetalRayTracingPipeline.h"
#include "Platform/Window.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);
    namespace
    {
        id<MTLTexture> CreateTextureView(id<MTLTexture> base_texture, GfxTexture const* gfx_texture, GfxTextureDescriptorDesc const* desc)
        {
            if (!desc || !base_texture)
            {
                return base_texture;
            }

            GfxTextureDesc const& texture_desc = gfx_texture->GetDesc();

            Uint32 first_mip = desc->first_mip;
            Uint32 mip_count = desc->mip_count == static_cast<Uint32>(-1) ? (texture_desc.mip_levels - first_mip) : desc->mip_count;
            Uint32 first_slice = desc->first_slice;
            Uint32 slice_count = desc->slice_count == static_cast<Uint32>(-1) ? (texture_desc.array_size - first_slice) : desc->slice_count;

            Bool needs_view = (first_mip != 0) ||
                             (mip_count != texture_desc.mip_levels) ||
                             (first_slice != 0) ||
                             (slice_count != texture_desc.array_size);

            if (!needs_view)
            {
                return base_texture;
            }

            NSRange mip_range = NSMakeRange(first_mip, mip_count);
            NSRange slice_range = NSMakeRange(first_slice, slice_count);

            id<MTLTexture> texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                                          textureType:base_texture.textureType
                                                                               levels:mip_range
                                                                               slices:slice_range];
            return texture_view;
        }
    }

    Bool MetalCapabilities::Initialize(GfxDevice* gfx)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        Bool supports_apple6 = [device supportsFamily:MTLGPUFamilyApple6];
        Bool supports_apple7 = [device supportsFamily:MTLGPUFamilyApple7];
        Bool supports_apple8 = [device supportsFamily:MTLGPUFamilyApple8];
        Bool supports_apple9 = [device supportsFamily:MTLGPUFamilyApple9];

        if (!supports_apple7)
        {
            ADRIA_LOG(ERROR, "Device doesn't support Apple GPU Family 7 which is required (minimum A15/M2)!");
            ADRIA_LOG(ERROR, "Shader Model 6.6 equivalent is required!");
            return false;
        }

        if (supports_apple6)
        {
            ray_tracing_support = RayTracingSupport::Tier1_0;
        }
        if (supports_apple7)
        {
            mesh_shader_support = MeshShaderSupport::Tier1;
        }

        vrs_support = VRSSupport::TierNotSupported;
        additional_shading_rates_supported = false;
        shading_rate_image_tile_size = 0;
        work_graph_support = WorkGraphSupport::TierNotSupported;

        if (supports_apple9)
        {
            shader_model = SM_6_8;
        }
        else if (supports_apple8)
        {
            shader_model = SM_6_7;
        }
        else if (supports_apple7)
        {
            shader_model = SM_6_6;
        }
        else if (supports_apple6)
        {
            shader_model = SM_6_5;
        }
        else
        {
            shader_model = SM_6_0; 
        }

        enhanced_barriers_supported = true;
        typed_uav_additional_formats_supported = true;

        ADRIA_LOG(INFO, "Metal Capabilities:");
        ADRIA_LOG(INFO, "  GPU: %s", [device.name UTF8String]);
        ADRIA_LOG(INFO, "  Ray Tracing: %s", ray_tracing_support != RayTracingSupport::TierNotSupported ? "Supported" : "Not Supported");
        ADRIA_LOG(INFO, "  Mesh Shaders: %s", mesh_shader_support != MeshShaderSupport::TierNotSupported ? "Supported" : "Not Supported");
        ADRIA_LOG(INFO, "  Shader Model Equivalent: SM_%d_%d",
            (shader_model - SM_6_0) / 10 + 6,
            (shader_model - SM_6_0) % 10);

        return true;
    }

    MetalDevice::MetalDevice(Window* _window) : window(_window)
    {
        device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            ADRIA_LOG(ERROR, "Failed to create Metal device!");
            return;
        }
        command_queue = [device newCommandQueue];
        argument_buffer = std::make_unique<MetalArgumentBuffer>(this, 4096);

        if (!capabilities.Initialize(this))
        {
            ADRIA_LOG(ERROR, "Device doesn't meet minimum requirements!");
            device = nil;
            command_queue = nil;
            return;
        }
    }

    MetalDevice::~MetalDevice()
    {
        @autoreleasepool
        {
            argument_buffer.reset();
            shader_library = nil;
            command_queue = nil;
            device = nil;
        }
    }

    void* MetalDevice::GetNative() const
    {
        return (__bridge void*)device;
    }

    void MetalDevice::OnResize(Uint32 w, Uint32 h)
    {
            ADRIA_TODO("Handle resizing");
    }

    GfxTexture* MetalDevice::GetBackbuffer() const
    {
        return nullptr;
    }

    Uint32 MetalDevice::GetBackbufferIndex() const
    {
        return frame_index;
    }

    Uint32 MetalDevice::GetFrameIndex() const
    {
        return frame_index;
    }

    GfxCapabilities const& MetalDevice::GetCapabilities() const
    {
        return capabilities;
    }

    GfxFence& MetalDevice::GetFence(GfxCommandListType type)
    {
        ADRIA_TODO("Revisit this");
        static GfxFence* static_dummy_fence = nullptr;
        return *static_dummy_fence; 
    }

    GfxShadingRateInfo const& MetalDevice::GetShadingRateInfo() const
    {
        return shading_rate_info;
    }

    GfxBindlessTable MetalDevice::AllocatePersistentBindlessTable(Uint32 count, GfxDescriptorType type)
    {
        return AllocateBindlessTable(count, type);
    }

    GfxBindlessTable MetalDevice::AllocateBindlessTable(Uint32 count, GfxDescriptorType type)
    {
        if (!argument_buffer)
        {
            return {};
        }

        Uint32 base_index = argument_buffer->AllocateRange(count);
        GfxBindlessTable table{};
        table.base = base_index;
        table.count = count;
        table.type = type;
        return table;
    }

    void MetalDevice::UpdateBindlessTable(GfxBindlessTable table, std::span<GfxDescriptor const> src_descriptors)
    {
        if (!table.IsValid() || src_descriptors.empty() || !argument_buffer)
        {
            return;
        }

        ADRIA_ASSERT_MSG(table.count == src_descriptors.size(), "Source descriptor count must match table size!");

        for (Uint32 i = 0; i < src_descriptors.size(); ++i)
        {
            MetalDescriptor src_desc = DecodeToMetalDescriptor(src_descriptors[i]);
            if (!src_desc.IsValid())
            {
                continue;
            }

            MetalResourceEntry const& src_entry = src_desc.parent_buffer->GetResourceEntry(src_desc.index);
            Uint32 dst_index = table.base + i;

            switch (src_entry.type)
            {
                case MetalResourceType::Texture:
                    if (src_entry.texture != nil)
                    {
                        argument_buffer->SetTexture(src_entry.texture, dst_index);
                    }
                    break;
                case MetalResourceType::Buffer:
                    if (src_entry.buffer != nil)
                    {
                        argument_buffer->SetBuffer(src_entry.buffer, dst_index, src_entry.buffer_offset);
                    }
                    break;
                case MetalResourceType::Sampler:
                    if (src_entry.sampler != nil)
                    {
                        argument_buffer->SetSampler(src_entry.sampler, dst_index);
                    }
                    break;
                case MetalResourceType::Unknown:
                default:
                    break;
            }
        }
    }

    void MetalDevice::UpdateBindlessTable(GfxBindlessTable table, Uint32 table_offset, GfxDescriptor src_descriptor, Uint32 src_count)
    {
        if (!table.IsValid() || !argument_buffer)
        {
            return;
        }

        MetalDescriptor src_desc = DecodeToMetalDescriptor(src_descriptor);
        if (!src_desc.IsValid())
        {
            return;
        }

        for (Uint32 i = 0; i < src_count; ++i)
        {
            Uint32 src_index = src_desc.index + i;
            Uint32 dst_index = table.base + table_offset + i;

            MetalResourceEntry const& src_entry = src_desc.parent_buffer->GetResourceEntry(src_index);

            switch (src_entry.type)
            {
                case MetalResourceType::Texture:
                    if (src_entry.texture != nil)
                    {
                        argument_buffer->SetTexture(src_entry.texture, dst_index);
                    }
                    break;
                case MetalResourceType::Buffer:
                    if (src_entry.buffer != nil)
                    {
                        argument_buffer->SetBuffer(src_entry.buffer, dst_index, src_entry.buffer_offset);
                    }
                    break;
                case MetalResourceType::Sampler:
                    if (src_entry.sampler != nil)
                    {
                        argument_buffer->SetSampler(src_entry.sampler, dst_index);
                    }
                    break;
                case MetalResourceType::Unknown:
                default:
                    break;
            }
        }
    }

    void MetalDevice::UpdateBindlessTables(std::vector<GfxBindlessTable> const& tables, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size)
    {
        if (tables.empty() || src_range_starts_and_size.empty() || !argument_buffer)
        {
            return;
        }

        ADRIA_ASSERT_MSG(tables.size() == src_range_starts_and_size.size(), "Tables and source ranges must have the same size!");

        for (Usize i = 0; i < tables.size(); ++i)
        {
            GfxBindlessTable const& table = tables[i];
            auto const& [src_descriptor, count] = src_range_starts_and_size[i];

            if (!table.IsValid())
            {
                continue;
            }

            MetalDescriptor src_desc = DecodeToMetalDescriptor(src_descriptor);
            if (!src_desc.IsValid())
            {
                continue;
            }

            for (Uint32 j = 0; j < count; ++j)
            {
                Uint32 src_index = src_desc.index + j;
                Uint32 dst_index = table.base + j;

                MetalResourceEntry const& src_entry = src_desc.parent_buffer->GetResourceEntry(src_index);

                switch (src_entry.type)
                {
                    case MetalResourceType::Texture:
                        if (src_entry.texture != nil)
                        {
                            argument_buffer->SetTexture(src_entry.texture, dst_index);
                        }
                        break;
                    case MetalResourceType::Buffer:
                        if (src_entry.buffer != nil)
                        {
                            argument_buffer->SetBuffer(src_entry.buffer, dst_index, src_entry.buffer_offset);
                        }
                        break;
                    case MetalResourceType::Sampler:
                        if (src_entry.sampler != nil)
                        {
                            argument_buffer->SetSampler(src_entry.sampler, dst_index);
                        }
                        break;
                    case MetalResourceType::Unknown:
                    default:
                        break;
                }
            }
        }
    }

    GfxDescriptor MetalDevice::CreateBufferSRV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
    {
        if (!buffer || !argument_buffer)
        {
            return {};
        }

        MetalBuffer const* metal_buffer = static_cast<MetalBuffer const*>(buffer);
        Uint32 index = argument_buffer->AllocateRange(1);

        argument_buffer->SetBuffer(metal_buffer->GetMetalBuffer(), index, 0);

        MetalDescriptor metal_desc{};
        metal_desc.parent_buffer = argument_buffer.get();
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
    {
        return CreateBufferSRV(buffer, desc);
    }

    GfxDescriptor MetalDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBuffer const* counter, GfxBufferDescriptorDesc const* desc)
    {
        return CreateBufferUAV(buffer, desc);
    }

    GfxDescriptor MetalDevice::CreateTextureSRV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture || !argument_buffer)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();
        id<MTLTexture> texture_view = CreateTextureView(base_texture, texture, desc);

        Uint32 index = argument_buffer->AllocateRange(1);
        argument_buffer->SetTexture(texture_view, index);

        MetalDescriptor metal_desc{};
        metal_desc.parent_buffer = argument_buffer.get();
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture || !argument_buffer)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();
        id<MTLTexture> texture_view = CreateTextureView(base_texture, texture, desc);

        Uint32 index = argument_buffer->AllocateRange(1);
        argument_buffer->SetTexture(texture_view, index);

        MetalDescriptor metal_desc{};
        metal_desc.parent_buffer = argument_buffer.get();
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateTextureRTV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();

        // For RTVs, create a view if needed (mip/slice selection)
        id<MTLTexture> rtv_texture = CreateTextureView(base_texture, texture, desc);

        // Encode the RTV descriptor
        MetalRenderTargetDescriptor rt_desc{};
        rt_desc.texture = rtv_texture;
        rt_desc.mip_level = desc ? desc->first_mip : 0;
        rt_desc.array_slice = desc ? desc->first_slice : 0;

        return EncodeFromMetalRenderTargetDescriptor(rt_desc);
    }

    GfxDescriptor MetalDevice::CreateTextureDSV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();

        // For DSVs, create a view if needed (mip/slice selection)
        id<MTLTexture> dsv_texture = CreateTextureView(base_texture, texture, desc);

        // Encode the DSV descriptor
        MetalRenderTargetDescriptor rt_desc{};
        rt_desc.texture = dsv_texture;
        rt_desc.mip_level = desc ? desc->first_mip : 0;
        rt_desc.array_slice = desc ? desc->first_slice : 0;

        return EncodeFromMetalRenderTargetDescriptor(rt_desc);
    }

    std::unique_ptr<GfxCommandList> MetalDevice::CreateCommandList(GfxCommandListType type)
    {
        return std::make_unique<MetalCommandList>(this, type);
    }

    std::unique_ptr<GfxTexture> MetalDevice::CreateTexture(GfxTextureDesc const& desc)
    {
        return std::make_unique<MetalTexture>(this, desc);
    }

    std::unique_ptr<GfxTexture> MetalDevice::CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data)
    {
        return std::make_unique<MetalTexture>(this, desc, data);
    }

    std::unique_ptr<GfxTexture> MetalDevice::CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer)
    {
        return std::make_unique<MetalTexture>(this, backbuffer, desc);
    }

    std::unique_ptr<GfxBuffer> MetalDevice::CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data)
    {
        return std::make_unique<MetalBuffer>(this, desc, initial_data);
    }

    std::unique_ptr<GfxBuffer> MetalDevice::CreateBuffer(GfxBufferDesc const& desc)
    {
        return std::make_unique<MetalBuffer>(this, desc);
    }

    std::unique_ptr<GfxPipelineState> MetalDevice::CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc)
    {
        return std::make_unique<MetalGraphicsPipelineState>(this, desc);
    }

    std::unique_ptr<GfxPipelineState> MetalDevice::CreateComputePipelineState(GfxComputePipelineStateDesc const& desc)
    {
        return std::make_unique<MetalComputePipelineState>(this, desc);
    }

    std::unique_ptr<GfxPipelineState> MetalDevice::CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc)
    {
        return std::make_unique<MetalMeshShadingPipelineState>(this, desc);
    }

    std::unique_ptr<GfxRayTracingTLAS> MetalDevice::CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
    {
        return std::make_unique<MetalRayTracingTLAS>(this, instances, flags);
    }

    std::unique_ptr<GfxRayTracingBLAS> MetalDevice::CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
    {
        return std::make_unique<MetalRayTracingBLAS>(this, geometries, flags);
    }

    std::unique_ptr<GfxRayTracingPipeline> MetalDevice::CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc)
    {
        return std::make_unique<MetalRayTracingPipeline>(this, desc);
    }

    void MetalDevice::RegisterBuffer(id<MTLBuffer> buffer)
    {
        if (!buffer) return;

        Uint64 base_address = [buffer gpuAddress];
        Uint64 size = [buffer length];

        BufferEntry entry{};
        entry.buffer = buffer;
        entry.base_address = base_address;
        entry.size = size;

        buffer_map[base_address] = entry;
    }

    void MetalDevice::UnregisterBuffer(id<MTLBuffer> buffer)
    {
        if (!buffer) 
        {
            return;
        }

        Uint64 base_address = [buffer gpuAddress];
        buffer_map.erase(base_address);
    }

    MetalDevice::BufferLookupResult MetalDevice::LookupBuffer(Uint64 gpu_address) const
    {
        BufferLookupResult result{};
        result.buffer = nil;
        result.offset = 0;

        if (buffer_map.empty())
        {
            return result;
        }

        auto it = buffer_map.upper_bound(gpu_address);
        if (it == buffer_map.begin())
        {
            return result;
        }
        --it;

        BufferEntry const& entry = it->second;
        if (gpu_address >= entry.base_address && gpu_address < entry.base_address + entry.size)
        {
            result.buffer = entry.buffer;
            result.offset = gpu_address - entry.base_address;
        }

        return result;
    }
}
