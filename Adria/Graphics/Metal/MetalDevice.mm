#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalDevice.h"
#include "MetalSwapchain.h"
#include "MetalTexture.h"
#include "MetalBuffer.h"
#include "MetalArgumentBuffer.h"
#include "MetalDescriptor.h"
#include "MetalRayTracingAS.h"
#include "MetalRayTracingPipeline.h"
#include "Platform/Window.h"

namespace adria
{
    MetalDevice::MetalDevice(Window* _window) : window(_window)
    {
        device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            ADRIA_TODO("Handle error");
            // Handle error - no Metal device available
            return;
        }
        command_queue = [device newCommandQueue];
        argument_buffer = std::make_unique<MetalArgumentBuffer>(this, 4096);
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

            MetalResourceType resource_type = src_desc.parent_buffer->GetResourceType(src_desc.index);
            Uint32 dst_index = table.base + i;

            ADRIA_TODO("How should this work?");
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

            ADRIA_TODO("How should this work?");
        }
    }

    void MetalDevice::UpdateBindlessTables(std::vector<GfxBindlessTable> const& tables, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size)
    {
        ADRIA_TODO("How should this work?");
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
        Uint32 index = argument_buffer->AllocateRange(1);

        argument_buffer->SetTexture(metal_texture->GetMetalTexture(), index);

        MetalDescriptor metal_desc{};
        metal_desc.parent_buffer = argument_buffer.get();
        metal_desc.index = index;

        return EncodeFromMetalDescriptor(metal_desc);
    }

    GfxDescriptor MetalDevice::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        return CreateTextureSRV(texture, desc);
    }

    GfxDescriptor MetalDevice::CreateTextureRTV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        // RTVs in Metal are not stored in argument buffers
        // They are set directly on MTLRenderPassDescriptor
    }

    GfxDescriptor MetalDevice::CreateTextureDSV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
    {
        // DSVs in Metal are not stored in argument buffers
        // They are set directly on MTLRenderPassDescriptor
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
}
