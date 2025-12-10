#include "MetalDescriptorAllocator.h"
#include "MetalDevice.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

    MetalDescriptorAllocator::MetalDescriptorAllocator(MetalDevice* device, Uint32 descriptor_count, std::string const& name)
        : metal_device(device), descriptor_count(descriptor_count)
    {
        id<MTLDevice> mtl_device = metal_device->GetMTLDevice();

        descriptor_buffer = [mtl_device newBufferWithLength:sizeof(IRDescriptorTableEntry) * descriptor_count
                                                    options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined | MTLResourceHazardTrackingModeTracked];

        descriptor_buffer.label = [NSString stringWithUTF8String:name.c_str()];

        cpu_address = descriptor_buffer.contents;
    }

    MetalDescriptorAllocator::~MetalDescriptorAllocator()
    {
        descriptor_buffer = nil;
    }

    Uint32 MetalDescriptorAllocator::Allocate(IRDescriptorTableEntry** descriptor)
    {
        Uint32 index = 0;

        if (!free_descriptors.empty())
        {
            index = free_descriptors.back();
            free_descriptors.pop_back();
        }
        else
        {
            if (allocated_count >= descriptor_count)
            {
                ADRIA_LOG(ERROR, "MetalDescriptorAllocator: Out of descriptors!");
                return UINT32_MAX;
            }
            index = allocated_count;
            ++allocated_count;
        }

        *descriptor = (IRDescriptorTableEntry*)((char*)cpu_address + sizeof(IRDescriptorTableEntry) * index);
        return index;
    }

    void MetalDescriptorAllocator::Free(Uint32 index)
    {
        if (index < allocated_count)
        {
            free_descriptors.push_back(index);
        }
    }
}