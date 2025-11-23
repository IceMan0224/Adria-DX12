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
#include "MetalFence.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
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

        // Create residency set for managing resource residency
        MTLResidencySetDescriptor* residency_desc = [MTLResidencySetDescriptor new];
        residency_desc.initialCapacity = 4096; // Start with reasonable capacity
        residency_set = [device newResidencySetWithDescriptor:residency_desc error:nil];
        residency_desc = nil;

        if (!residency_set)
        {
            ADRIA_LOG(ERROR, "Failed to create residency set!");
            device = nil;
            return;
        }

        command_queue = [device newCommandQueue];
        [command_queue addResidencySet:residency_set]; // Attach residency set to queue

        argument_buffer = std::make_unique<MetalArgumentBuffer>(this, 4096);

        if (!capabilities.Initialize(this))
        {
            ADRIA_LOG(ERROR, "Device doesn't meet minimum requirements!");
            device = nil;
            command_queue = nil;
            return;
        }

        if (window)
        {
            swapchain = std::make_unique<MetalSwapchain>(
                this,
                window->Handle(),
                window->Width(),
                window->Height()
            );
        }

        for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
        {
            graphics_cmd_list_pool[i] = std::make_unique<GfxGraphicsCommandListPool>(this);
            compute_cmd_list_pool[i] = std::make_unique<GfxComputeCommandListPool>(this);
            copy_cmd_list_pool[i] = std::make_unique<GfxCopyCommandListPool>(this);
        }

        for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
        {
            dynamic_allocators.emplace_back(new GfxLinearDynamicAllocator(this, 1 << 20));
        }
        dynamic_allocator_on_init.reset(new GfxLinearDynamicAllocator(this, 1 << 30));
    }

    MetalDevice::~MetalDevice()
    {
        @autoreleasepool
        {
            buffer_map.clear();
            swapchain.reset();
            argument_buffer.reset();
            shader_library = nil;

            residency_set = nil;

            command_queue = nil;
            device = nil;
        }
    }

    void* MetalDevice::GetNative() const
    {
        return (__bridge void*)device;
    }

    void* MetalDevice::GetWindowHandle() const
    {
        return window ? window->Handle() : nullptr;
    }

    void MetalDevice::OnResize(Uint32 w, Uint32 h)
    {
        if (swapchain)
        {
            swapchain->OnResize(w, h);
        }
    }

    GfxTexture* MetalDevice::GetBackbuffer() const
    {
        if (swapchain)
        {
            return swapchain->GetBackbuffer();
        }
        return nullptr;
    }

    Uint32 MetalDevice::GetBackbufferIndex() const
    {
        if (swapchain)
        {
            return swapchain->GetBackbufferIndex();
        }
        return frame_index;
    }

    Uint32 MetalDevice::GetFrameIndex() const
    {
        return frame_index;
    }

    void MetalDevice::SetRenderingNotStarted()
    {
        rendering_not_started = true;
        dynamic_allocator_on_init.reset(new GfxLinearDynamicAllocator(this, 1 << 30));
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

    GfxCommandList* MetalDevice::GetCommandList(GfxCommandListType type) const
    {
        Uint32 backbuffer_index = swapchain ? swapchain->GetBackbufferIndex() : 0;
        switch (type)
        {
        case GfxCommandListType::Graphics:
            return graphics_cmd_list_pool[backbuffer_index]->GetMainCmdList();
        case GfxCommandListType::Compute:
            return compute_cmd_list_pool[backbuffer_index]->GetMainCmdList();
        case GfxCommandListType::Copy:
            return copy_cmd_list_pool[backbuffer_index]->GetMainCmdList();
        default:
            return nullptr;
        }
    }

    GfxCommandList* MetalDevice::GetLatestCommandList(GfxCommandListType type) const
    {
        Uint32 backbuffer_index = swapchain ? swapchain->GetBackbufferIndex() : 0;
        switch (type)
        {
        case GfxCommandListType::Graphics:
            return graphics_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
        case GfxCommandListType::Compute:
            return compute_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
        case GfxCommandListType::Copy:
            return copy_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
        default:
            return nullptr;
        }
    }

    GfxCommandList* MetalDevice::AllocateCommandList(GfxCommandListType type) const
    {
        Uint32 backbuffer_index = swapchain ? swapchain->GetBackbufferIndex() : 0;
        switch (type)
        {
        case GfxCommandListType::Graphics:
            return graphics_cmd_list_pool[backbuffer_index]->AllocateCmdList();
        case GfxCommandListType::Compute:
            return compute_cmd_list_pool[backbuffer_index]->AllocateCmdList();
        case GfxCommandListType::Copy:
            return copy_cmd_list_pool[backbuffer_index]->AllocateCmdList();
        default:
            return nullptr;
        }
    }

    void MetalDevice::FreeCommandList(GfxCommandList* cmd_list, GfxCommandListType type)
    {
        Uint32 backbuffer_index = swapchain ? swapchain->GetBackbufferIndex() : 0;
        switch (type)
        {
        case GfxCommandListType::Graphics:
            graphics_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
            break;
        case GfxCommandListType::Compute:
            compute_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
            break;
        case GfxCommandListType::Copy:
            copy_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
            break;
        }
    }

    GfxLinearDynamicAllocator* MetalDevice::GetDynamicAllocator() const
    {
        return rendering_not_started ? dynamic_allocator_on_init.get() : dynamic_allocators[swapchain ? swapchain->GetBackbufferIndex() : 0].get();
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

        id<MTLTexture> rtv_texture = CreateTextureView(base_texture, texture, desc);
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
        id<MTLTexture> dsv_texture = CreateTextureView(base_texture, texture, desc);

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

    std::shared_ptr<GfxBuffer> MetalDevice::CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& initial_data)
    {
        return std::make_shared<MetalBuffer>(this, desc, initial_data);
    }

    std::shared_ptr<GfxBuffer> MetalDevice::CreateBufferShared(GfxBufferDesc const& desc)
    {
        return std::make_shared<MetalBuffer>(this, desc);
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

    std::unique_ptr<GfxFence> MetalDevice::CreateFence(Char const* name)
    {
        std::unique_ptr<MetalFence> fence = std::make_unique<MetalFence>();
        if (!fence->Create(this, name))
        {
            return nullptr;
        }
        return fence;
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

    void MetalDevice::BeginFrame()
    {
        if (!swapchain)
        {
            return;
        }

        // Process eviction queue - remove allocations that are old enough
        while (!eviction_queue.empty())
        {
            EvictionEntry const& entry = eviction_queue.front();
            if (entry.frame_id + GFX_BACKBUFFER_COUNT > frame_index)
            {
                break; // Too recent, keep in queue
            }

            [residency_set removeAllocation:entry.buffer_or_texture];
            residency_dirty = true;
            eviction_queue.pop();
        }

        if (residency_dirty)
        {
            [residency_set commit];
            residency_dirty = false;
        }

        if (rendering_not_started)
        {
            dynamic_allocator_on_init.reset();
            first_frame = true;
            rendering_not_started = false;
        }

        // Acquire the drawable for this frame
        id<CAMetalDrawable> drawable = GetCurrentDrawable();
        if (!drawable)
        {
            return;
        }

        Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
        graphics_cmd_list_pool[backbuffer_index]->BeginCmdLists();
        compute_cmd_list_pool[backbuffer_index]->BeginCmdLists();
        copy_cmd_list_pool[backbuffer_index]->BeginCmdLists();
        dynamic_allocators[backbuffer_index]->Clear();

        first_frame = false;
    }

    void MetalDevice::EndFrame()
    {
        if (!swapchain)
        {
            return;
        }

        Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
        graphics_cmd_list_pool[backbuffer_index]->EndCmdLists();
        compute_cmd_list_pool[backbuffer_index]->EndCmdLists();
        copy_cmd_list_pool[backbuffer_index]->EndCmdLists();

        // Submit all command lists from the pools (matching D3D12's ExecuteCommandListPool behavior)
        for (auto& cmd_list : *graphics_cmd_list_pool[backbuffer_index])
        {
            cmd_list->End();
            cmd_list->Submit();
            cmd_list->Begin();
        }

        for (auto& cmd_list : *compute_cmd_list_pool[backbuffer_index])
        {
            cmd_list->End();
            cmd_list->Submit();
            cmd_list->Begin();
        }

        for (auto& cmd_list : *copy_cmd_list_pool[backbuffer_index])
        {
            cmd_list->End();
            cmd_list->Submit();
            cmd_list->Begin();
        }

        // Create a dedicated command buffer for presentation
        id<CAMetalDrawable> drawable = GetCurrentDrawable();
        if (drawable)
        {
            id<MTLCommandBuffer> present_cmd_buffer = [command_queue commandBuffer];
            [present_cmd_buffer presentDrawable:drawable];
            [present_cmd_buffer commit];
        }

        swapchain->Present(true);
        frame_index++;
    }

    id<CAMetalDrawable> MetalDevice::GetCurrentDrawable()
    {
        if (swapchain)
        {
            return swapchain->GetCurrentDrawable();
        }
        return nil;
    }

    void MetalDevice::MakeResident(id<MTLBuffer> buffer)
    {
        if (buffer && residency_set)
        {
            [residency_set addAllocation:buffer];
            residency_dirty = true;
            [residency_set commit];
            residency_dirty = false;
        }
    }

    void MetalDevice::MakeResident(id<MTLTexture> texture)
    {
        if (texture && residency_set)
        {
            [residency_set addAllocation:texture];
            residency_dirty = true;
            [residency_set commit];
            residency_dirty = false;
        }
    }

    void MetalDevice::Evict(id<MTLBuffer> buffer)
    {
        if (buffer && residency_set)
        {
            EvictionEntry entry;
            entry.buffer_or_texture = buffer;
            entry.frame_id = frame_index;
            eviction_queue.push(entry);
        }
    }

    void MetalDevice::Evict(id<MTLTexture> texture)
    {
        if (texture && residency_set)
        {
            EvictionEntry entry;
            entry.buffer_or_texture = texture;
            entry.frame_id = frame_index;
            eviction_queue.push(entry);
        }
    }
}
