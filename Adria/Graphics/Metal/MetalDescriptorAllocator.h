#pragma once
#import <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

namespace adria
{
    class MetalDevice;

    class MetalDescriptorAllocator
    {
    public:
        MetalDescriptorAllocator(MetalDevice* device, Uint32 descriptor_count, std::string const& name);
        ~MetalDescriptorAllocator();

        Uint32 Allocate(IRDescriptorTableEntry** descriptor);
        void Free(Uint32 index);

        id<MTLBuffer> GetBuffer() const { return descriptor_buffer; }

    private:
        MetalDevice* metal_device = nullptr;
        id<MTLBuffer> descriptor_buffer = nil;
        void* cpu_address = nullptr;
        Uint32 descriptor_count = 0;
        Uint32 allocated_count = 0;
        std::vector<Uint32> free_descriptors;
    };
}