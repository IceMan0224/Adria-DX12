#import <Metal/Metal.h>
#include "MetalBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    MetalBuffer::MetalBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data)
        : GfxBuffer(gfx, desc)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = (__bridge id<MTLDevice>)metal_device->GetNative();

        MTLResourceOptions options = MTLResourceStorageModeShared;
        metal_buffer = [device newBufferWithLength:desc.size options:options];
        mapped_data = [metal_buffer contents];
        if (initial_data)
        {
            memcpy(mapped_data, initial_data, desc.size);
        }

        metal_device->RegisterBuffer(metal_buffer);
        metal_device->MakeResident(metal_buffer);
    }

    MetalBuffer::~MetalBuffer()
    {
        if (metal_buffer)
        {
            MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
            metal_device->UnregisterBuffer(metal_buffer);
            metal_device->Evict(metal_buffer); 
            metal_buffer = nil;
        }
    }

    void* MetalBuffer::GetNative() const
    {
        return (__bridge void*)metal_buffer;
    }

    Uint64 MetalBuffer::GetGpuAddress() const
    {
        return (Uint64)[metal_buffer gpuAddress];
    }

    void* MetalBuffer::GetSharedHandle() const
    {
        return nullptr;
    }

    void* MetalBuffer::Map()
    {
        return [metal_buffer contents];
    }

    void MetalBuffer::Unmap()
    {
    }

    void MetalBuffer::SetName(Char const* name)
    {
        [metal_buffer setLabel:[NSString stringWithUTF8String:name]];
    }
}
