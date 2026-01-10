#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "MetalDevice.h"
#include "MetalSwapchain.h"
#include "MetalTexture.h"
#include "MetalBuffer.h"
#include "MetalCommandList.h"
#include "MetalPipelineState.h"
#include "MetalDescriptor.h"
#include "MetalRingDescriptorAllocator.h"
#include "MetalConversions.h"
#include "MetalRayTracingAS.h"
#include "MetalRayTracingPipeline.h"
#include "MetalFence.h"
#include "MetalQueryHeap.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Platform/Window.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

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

        MTLResidencySetDescriptor* residency_desc = [MTLResidencySetDescriptor new];
        residency_desc.initialCapacity = 4096; 
        residency_set = [device newResidencySetWithDescriptor:residency_desc error:nil];
        residency_desc = nil;

        if (!residency_set)
        {
            ADRIA_LOG(ERROR, "Failed to create residency set!");
            device = nil;
            return;
        }

        command_queue = [device newCommandQueue];
        [command_queue addResidencySet:residency_set]; 

        resource_descriptor_allocator = std::make_unique<MetalRingDescriptorAllocator>(this, 65536, 2048, "Resource Descriptor Allocator");

        if (!capabilities.Initialize(this))
        {
            ADRIA_LOG(ERROR, "Device doesn't meet minimum requirements!");
            device = nil;
            command_queue = nil;
            return;
        }

        CreateStaticSamplers();
        CreateClearPipelines();

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

        dummy_fence = new MetalFence();
        dummy_fence->Create(this, "Dummy Fence");
    }

    MetalDevice::~MetalDevice()
    {
        @autoreleasepool
        {
            buffer_map.clear();
            swapchain.reset();
            resource_descriptor_allocator.reset();
            shader_library = nil;
            sampler_table_buffer = nil;
            for (Uint32 i = 0; i < STATIC_SAMPLER_COUNT; ++i)
            {
                static_samplers[i] = nil;
            }
            for (Uint32 i = 0; i < CLEAR_PIPELINE_COUNT; ++i)
            {
                clear_pipelines[i] = nil;
            }
            clear_library = nil;

            delete dummy_fence;
            dummy_fence = nullptr;

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
        return *dummy_fence;
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

    void MetalDevice::FreeCPUDescriptor(GfxDescriptor descriptor)
    {
        
    }

    Uint32 MetalDevice::GetBindlessDescriptorIndex(GfxDescriptor descriptor) const
    {
        MetalDescriptor metal_desc = DecodeToMetalDescriptor(descriptor);
        if (!metal_desc.IsValid())
        {
            return Uint32(-1);
        }
        return metal_desc.index;
    }

    GfxDescriptor MetalDevice::CreateBufferSRV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
    {
        if (!buffer || !resource_descriptor_allocator)
        {
            return {};
        }

        MetalBuffer const* metal_buffer = static_cast<MetalBuffer const*>(buffer);
        id<MTLBuffer> mtl_buffer = metal_buffer->GetMetalBuffer();

        IRDescriptorTableEntry* entry = nullptr;
        Uint32 index = UINT32_MAX;

        if (buffer->IsPersistent())
        {
            index = AllocatePersistentResourceDescriptor(&entry);
        }
        else
        {
            index = AllocateResourceDescriptor(&entry);
        }

        if (index == UINT32_MAX || !entry)
        {
            return {};
        }

        // Set the buffer in the descriptor entry using GPU address
        Uint64 buffer_address = metal_buffer->GetGpuAddress();
        Uint64 offset = desc ? desc->offset : 0;
        IRDescriptorTableSetBuffer(entry, buffer_address + offset, 0);

        MetalDescriptor metal_desc{};
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
    {
        if (!buffer || !resource_descriptor_allocator)
        {
            return {};
        }

        MetalBuffer const* metal_buffer = static_cast<MetalBuffer const*>(buffer);

        IRDescriptorTableEntry* entry = nullptr;
        Uint32 index = UINT32_MAX;

        // Use persistent or transient allocation based on buffer's persistence flag
        if (buffer->IsPersistent())
        {
            index = AllocatePersistentResourceDescriptor(&entry);
        }
        else
        {
            index = AllocateResourceDescriptor(&entry);
        }

        if (index == UINT32_MAX || !entry)
        {
            return {};
        }

        // Set the buffer in the descriptor entry using GPU address
        Uint64 buffer_address = metal_buffer->GetGpuAddress();
        Uint64 offset = desc ? desc->offset : 0;
        IRDescriptorTableSetBuffer(entry, buffer_address + offset, 0);

        MetalDescriptor metal_desc{};
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBuffer const* counter, GfxBufferDescriptorDesc const* desc)
    {
        return CreateBufferUAV(buffer, desc);
    }

    GfxDescriptor MetalDevice::CreateTextureSRV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture || !resource_descriptor_allocator)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();
        id<MTLTexture> texture_view = CreateTextureView(base_texture, texture, desc);

        IRDescriptorTableEntry* entry = nullptr;
        Uint32 index = UINT32_MAX;

        // Use persistent or transient allocation based on texture's persistence flag
        if (texture->IsPersistent())
        {
            index = AllocatePersistentResourceDescriptor(&entry);
        }
        else
        {
            index = AllocateResourceDescriptor(&entry);
        }

        if (index == UINT32_MAX || !entry)
        {
            return {};
        }

        IRDescriptorTableSetTexture(entry, texture_view, 0.0f, 0);
        MetalDescriptor metal_desc{};
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!texture || !resource_descriptor_allocator)
        {
            return {};
        }

        MetalTexture const* metal_texture = static_cast<MetalTexture const*>(texture);
        id<MTLTexture> base_texture = metal_texture->GetMetalTexture();

        id<MTLTexture> texture_view = nil;
        GfxTextureDesc const& tex_desc = texture->GetDesc();
        if (HasFlag(tex_desc.misc_flags, GfxTextureMiscFlag::TextureCube))
        {
            Uint32 first_mip = desc ? desc->first_mip : 0;
            Uint32 mip_count = desc ? (desc->mip_count == static_cast<Uint32>(-1) ? tex_desc.mip_levels - first_mip : desc->mip_count) : tex_desc.mip_levels;
            Uint32 first_slice = desc ? desc->first_slice : 0;
            Uint32 slice_count = desc ? (desc->slice_count == static_cast<Uint32>(-1) ? tex_desc.array_size - first_slice : desc->slice_count) : tex_desc.array_size;

            texture_view = [base_texture newTextureViewWithPixelFormat:base_texture.pixelFormat
                                                           textureType:MTLTextureType2DArray
                                                                levels:NSMakeRange(first_mip, mip_count)
                                                                slices:NSMakeRange(first_slice, slice_count)];
            MakeResident(texture_view);
        }
        else
        {
            texture_view = CreateTextureView(base_texture, texture, desc);
        }

        IRDescriptorTableEntry* entry = nullptr;
        Uint32 index = UINT32_MAX;

        if (texture->IsPersistent())
        {
            index = AllocatePersistentResourceDescriptor(&entry);
        }
        else
        {
            index = AllocateResourceDescriptor(&entry);
        }

        if (index == UINT32_MAX || !entry)
        {
            return {};
        }

        IRDescriptorTableSetTexture(entry, texture_view, 0.0f, 0);

        MetalDescriptor metal_desc{};
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

    std::unique_ptr<GfxQueryHeap> MetalDevice::CreateQueryHeap(GfxQueryHeapDesc const& desc)
    {
        auto heap = std::make_unique<MetalQueryHeap>(this, desc);
        if (!heap->IsValid())
        {
            return nullptr;
        }
        return heap;
    }

    void MetalDevice::GetTimestampFrequency(Uint64& frequency) const
    {
        frequency = 1000000000ULL;
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

        // Release completed frame descriptors (like D3D12)
        if (resource_descriptor_allocator)
        {
            // Use frame_index - GFX_BACKBUFFER_COUNT as completed frame
            // This ensures we don't free descriptors still in use by GPU
            if (frame_index >= GFX_BACKBUFFER_COUNT)
            {
                resource_descriptor_allocator->ReleaseCompletedFrames(frame_index - GFX_BACKBUFFER_COUNT);
            }
        }

        while (!eviction_queue.empty())
        {
            EvictionEntry const& entry = eviction_queue.front();
            if (entry.frame_id + GFX_BACKBUFFER_COUNT > frame_index)
            {
                break; 
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

        id<CAMetalDrawable> drawable = GetCurrentDrawable();
        if (drawable)
        {
            id<MTLCommandBuffer> present_cmd_buffer = [command_queue commandBuffer];
            [present_cmd_buffer presentDrawable:drawable];
            [present_cmd_buffer commit];
        }

        swapchain->Present(true);

        // Finish current frame for descriptor allocator (like D3D12)
        if (resource_descriptor_allocator)
        {
            resource_descriptor_allocator->FinishCurrentFrame(frame_index);
        }

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

    Uint32 MetalDevice::AllocateResourceDescriptor(IRDescriptorTableEntry** descriptor)
    {
        if (!resource_descriptor_allocator)
        {
            return UINT32_MAX;
        }
        return resource_descriptor_allocator->AllocateTransient(descriptor);
    }

    Uint32 MetalDevice::AllocatePersistentResourceDescriptor(IRDescriptorTableEntry** descriptor)
    {
        if (!resource_descriptor_allocator)
        {
            return UINT32_MAX;
        }
        return resource_descriptor_allocator->AllocateReserved(descriptor);
    }

    void MetalDevice::FreeResourceDescriptor(Uint32 index)
    {
        // Ring allocator handles automatic freeing - no manual free needed
        // This is kept for API compatibility but does nothing
        (void)index;
    }

    id<MTLBuffer> MetalDevice::GetResourceDescriptorBuffer() const
    {
        if (!resource_descriptor_allocator)
        {
            return nil;
        }
        return resource_descriptor_allocator->GetBuffer();
    }

    void MetalDevice::CreateStaticSamplers()
    {
        sampler_table_buffer = [device newBufferWithLength:sizeof(IRDescriptorTableEntry) * STATIC_SAMPLER_COUNT
                                       options:MTLResourceStorageModeShared | MTLResourceHazardTrackingModeTracked];
        sampler_table_buffer.label = @"Static Sampler Table";
        MakeResident(sampler_table_buffer);
        sampler_table_gpu_address = [sampler_table_buffer gpuAddress];

        IRDescriptorTableEntry* entries = (IRDescriptorTableEntry*)sampler_table_buffer.contents;
        auto EncodeSampler = [&, this](Uint32 index, MTLSamplerMinMagFilter minMag, MTLSamplerMipFilter mip,
                                  MTLSamplerAddressMode addressU, MTLSamplerAddressMode addressV, MTLSamplerAddressMode addressW,
                                  MTLCompareFunction compareFunc, NSUInteger maxAniso, MTLSamplerBorderColor borderColor) {
            MTLSamplerDescriptor* desc = [MTLSamplerDescriptor new];
            desc.minFilter = minMag;
            desc.magFilter = minMag;
            desc.mipFilter = mip;
            desc.sAddressMode = addressU;
            desc.tAddressMode = addressV;
            desc.rAddressMode = addressW;
            desc.compareFunction = compareFunc;
            desc.maxAnisotropy = maxAniso;
            desc.borderColor = borderColor;
            desc.lodMinClamp = 0.0f;
            desc.lodMaxClamp = FLT_MAX;
            desc.supportArgumentBuffers = YES;

            id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:desc];
            static_samplers[index] = sampler;  
            IRDescriptorTableSetSampler(&entries[index], sampler, 0.0f);
        };

        // s0: LinearWrapSampler
        EncodeSampler(0, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s1: LinearClampSampler
        EncodeSampler(1, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s2: LinearBorderSampler
        EncodeSampler(2, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeClampToBorderColor,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s3: PointWrapSampler
        EncodeSampler(3, MTLSamplerMinMagFilterNearest, MTLSamplerMipFilterNearest,
                      MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s4: PointClampSampler
        EncodeSampler(4, MTLSamplerMinMagFilterNearest, MTLSamplerMipFilterNearest,
                      MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s5: PointBorderSampler
        EncodeSampler(5, MTLSamplerMinMagFilterNearest, MTLSamplerMipFilterNearest,
                      MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeClampToBorderColor,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s6: ShadowClampSampler (comparison)
        EncodeSampler(6, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToEdge,
                      MTLCompareFunctionLessEqual, 1, MTLSamplerBorderColorOpaqueWhite);

        // s7: ShadowWrapSampler (comparison)
        EncodeSampler(7, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat,
                      MTLCompareFunctionLessEqual, 1, MTLSamplerBorderColorOpaqueWhite);

        // s8: LinearMirrorSampler
        EncodeSampler(8, MTLSamplerMinMagFilterLinear, MTLSamplerMipFilterLinear,
                      MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeRepeat,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        // s9: PointMirrorSampler
        EncodeSampler(9, MTLSamplerMinMagFilterNearest, MTLSamplerMipFilterNearest,
                      MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeRepeat,
                      MTLCompareFunctionNever, 1, MTLSamplerBorderColorOpaqueBlack);

        ADRIA_LOG(INFO, "Created %u static samplers for Metal (GPU address: 0x%llx)", STATIC_SAMPLER_COUNT, sampler_table_gpu_address);
    }

    void MetalDevice::CreateClearPipelines()
    {
        NSString* shader_source = @R"(
#include <metal_stdlib>
using namespace metal;

struct ClearBufferParams
{
    uint4 clear_value;
    uint  element_count;
};

struct ClearTextureParams
{
    uint4 clear_value_uint;
    float4 clear_value_float;
    uint3 dimensions;
};

kernel void ClearBufferUint(device uint* buffer [[buffer(0)]], constant ClearBufferParams& params [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.element_count) return;
    buffer[tid] = params.clear_value.x;
}

kernel void ClearBufferUint4(device uint4* buffer [[buffer(0)]], constant ClearBufferParams& params [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.element_count) return;
    buffer[tid] = params.clear_value;
}

kernel void ClearBufferFloat(device float* buffer [[buffer(0)]], constant ClearBufferParams& params [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.element_count) return;
    buffer[tid] = as_type<float>(params.clear_value.x);
}

kernel void ClearBufferFloat4(device float4* buffer [[buffer(0)]], constant ClearBufferParams& params [[buffer(1)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.element_count) return;
    buffer[tid] = as_type<float4>(params.clear_value);
}

kernel void ClearTexture1DFloat(texture1d<float, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.dimensions.x) return;
    tex.write(params.clear_value_float, tid);
}

kernel void ClearTexture1DUint(texture1d<uint, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.dimensions.x) return;
    tex.write(params.clear_value_uint, tid);
}

kernel void ClearTexture1DInt(texture1d<int, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint tid [[thread_position_in_grid]])
{
    if (tid >= params.dimensions.x) return;
    tex.write(as_type<int4>(params.clear_value_uint), tid);
}

kernel void ClearTexture1DArrayFloat(texture1d_array<float, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(params.clear_value_float, tid.x, tid.y);
}

kernel void ClearTexture1DArrayUint(texture1d_array<uint, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(params.clear_value_uint, tid.x, tid.y);
}

kernel void ClearTexture1DArrayInt(texture1d_array<int, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(as_type<int4>(params.clear_value_uint), tid.x, tid.y);
}

kernel void ClearTexture2DFloat(texture2d<float, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(params.clear_value_float, tid);
}

kernel void ClearTexture2DUint(texture2d<uint, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(params.clear_value_uint, tid);
}

kernel void ClearTexture2DInt(texture2d<int, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint2 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y) return;
    tex.write(as_type<int4>(params.clear_value_uint), tid);
}

kernel void ClearTexture2DArrayFloat(texture2d_array<float, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(params.clear_value_float, tid.xy, tid.z);
}

kernel void ClearTexture2DArrayUint(texture2d_array<uint, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(params.clear_value_uint, tid.xy, tid.z);
}

kernel void ClearTexture2DArrayInt(texture2d_array<int, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(as_type<int4>(params.clear_value_uint), tid.xy, tid.z);
}

kernel void ClearTexture3DFloat(texture3d<float, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(params.clear_value_float, tid);
}

kernel void ClearTexture3DUint(texture3d<uint, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(params.clear_value_uint, tid);
}

kernel void ClearTexture3DInt(texture3d<int, access::write> tex [[texture(10)]], constant ClearTextureParams& params [[buffer(10)]], uint3 tid [[thread_position_in_grid]])
{
    if (tid.x >= params.dimensions.x || tid.y >= params.dimensions.y || tid.z >= params.dimensions.z) return;
    tex.write(as_type<int4>(params.clear_value_uint), tid);
}
)";

        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion3_0;

        clear_library = [device newLibraryWithSource:shader_source options:options error:&error];
        if (error || !clear_library)
        {
            ADRIA_LOG(ERROR, "Failed to compile clear shaders: %s", [[error localizedDescription] UTF8String]);
            return;
        }

        Char const* kernel_names[] = {
            "ClearBufferUint",
            "ClearBufferUint4",
            "ClearBufferFloat",
            "ClearBufferFloat4",
            "ClearTexture1DFloat",
            "ClearTexture1DUint",
            "ClearTexture1DInt",
            "ClearTexture1DArrayFloat",
            "ClearTexture1DArrayUint",
            "ClearTexture1DArrayInt",
            "ClearTexture2DFloat",
            "ClearTexture2DUint",
            "ClearTexture2DInt",
            "ClearTexture2DArrayFloat",
            "ClearTexture2DArrayUint",
            "ClearTexture2DArrayInt",
            "ClearTexture3DFloat",
            "ClearTexture3DUint",
            "ClearTexture3DInt"
        };

        for (Uint32 i = 0; i < CLEAR_PIPELINE_COUNT; ++i)
        {
            id<MTLFunction> function = [clear_library newFunctionWithName:[NSString stringWithUTF8String:kernel_names[i]]];
            if (!function)
            {
                ADRIA_LOG(ERROR, "Failed to find clear kernel: %s", kernel_names[i]);
                continue;
            }

            clear_pipelines[i] = [device newComputePipelineStateWithFunction:function error:&error];
            if (error || !clear_pipelines[i])
            {
                ADRIA_LOG(ERROR, "Failed to create clear pipeline for %s: %s", kernel_names[i], [[error localizedDescription] UTF8String]);
            }
        }

        ADRIA_LOG(INFO, "Created %u clear pipelines for Metal", CLEAR_PIPELINE_COUNT);
    }

    id<MTLTexture> MetalDevice::CreateTextureView(id<MTLTexture> base_texture, GfxTexture const* gfx_texture, GfxTextureDescriptorDesc const* desc)
    {
        if (!base_texture || !desc)
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
