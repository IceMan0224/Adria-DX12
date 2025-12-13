#include "MetalRingDescriptorAllocator.h"
#include "MetalDevice.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

    MetalRingDescriptorAllocator::MetalRingDescriptorAllocator(MetalDevice* device, Uint32 descriptor_count, Uint32 reserved_count, std::string const& name)
        : metal_device(device), descriptor_count(descriptor_count), reserved_count(reserved_count),
          ring_allocator(descriptor_count - reserved_count, 0)
    {
        id<MTLDevice> mtl_device = metal_device->GetMTLDevice();

        descriptor_buffer = [mtl_device newBufferWithLength:sizeof(IRDescriptorTableEntry) * descriptor_count
                                                    options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined | MTLResourceHazardTrackingModeTracked];

        descriptor_buffer.label = [NSString stringWithUTF8String:name.c_str()];
        cpu_address = descriptor_buffer.contents;

        metal_device->MakeResident(descriptor_buffer);
    }

    MetalRingDescriptorAllocator::~MetalRingDescriptorAllocator()
    {
        metal_device->Evict(descriptor_buffer);
        descriptor_buffer = nil;
    }

    Uint32 MetalRingDescriptorAllocator::AllocateReserved(IRDescriptorTableEntry** descriptor)
    {
        if (reserved_allocated >= reserved_count)
        {
            ADRIA_LOG(ERROR, "MetalRingDescriptorAllocator: Out of reserved descriptors!");
            return UINT32_MAX;
        }

        Uint32 index = reserved_allocated++;
        *descriptor = (IRDescriptorTableEntry*)((char*)cpu_address + sizeof(IRDescriptorTableEntry) * index);
        return index;
    }

    Uint32 MetalRingDescriptorAllocator::AllocateTransient(IRDescriptorTableEntry** descriptor, Uint32 count)
    {
        Uint64 ring_offset = ring_allocator.Allocate(count);
        if (ring_offset == INVALID_ALLOC_OFFSET)
        {
            ADRIA_LOG(ERROR, "MetalRingDescriptorAllocator: Ring buffer full! Used: %llu/%llu",
                      ring_allocator.UsedSize(), ring_allocator.MaxSize());
            return UINT32_MAX;
        }

        Uint32 index = reserved_count + static_cast<Uint32>(ring_offset);
        *descriptor = (IRDescriptorTableEntry*)((char*)cpu_address + sizeof(IRDescriptorTableEntry) * index);
        return index;
    }

    void MetalRingDescriptorAllocator::FinishCurrentFrame(Uint64 frame)
    {
        ring_allocator.FinishCurrentFrame(frame);
    }

    void MetalRingDescriptorAllocator::ReleaseCompletedFrames(Uint64 completed_frame)
    {
        ring_allocator.ReleaseCompletedFrames(completed_frame);
    }
}