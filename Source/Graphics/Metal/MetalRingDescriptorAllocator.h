#pragma once
#import <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "Utilities/RingOffsetAllocator.h"

namespace adria
{
    class MetalDevice;

    class MetalRingDescriptorAllocator
    {
    public:
        MetalRingDescriptorAllocator(MetalDevice* device, Uint32 descriptor_count, Uint32 reserved_count, std::string const& name);
        ~MetalRingDescriptorAllocator();

        Uint32 AllocateReserved(IRDescriptorTableEntry** descriptor);
        Uint32 AllocateTransient(IRDescriptorTableEntry** descriptor, Uint32 count = 1);

        void FinishCurrentFrame(Uint64 frame);
        void ReleaseCompletedFrames(Uint64 completed_frame);

        id<MTLBuffer> GetBuffer() const { return descriptor_buffer; }
        Uint32 GetReservedCount() const { return reserved_count; }

    private:
        MetalDevice* metal_device = nullptr;
        id<MTLBuffer> descriptor_buffer = nil;
        void* cpu_address = nullptr;
        Uint32 descriptor_count = 0;
        Uint32 reserved_count = 0;
        Uint32 reserved_allocated = 0;
        RingOffsetAllocator ring_allocator;
    };
}