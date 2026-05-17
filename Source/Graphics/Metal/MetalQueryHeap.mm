#import <Metal/Metal.h>
#include "MetalQueryHeap.h"
#include "MetalDevice.h"

namespace adria
{
    MetalQueryHeap::MetalQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc)
        : GfxQueryHeap(gfx, desc)
    {
        if (desc.type != GfxQueryType::Timestamp)
        {
            return;
        }

        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        id<MTLCounterSet> timestamp_counter_set = nil;
        for (id<MTLCounterSet> cs in device.counterSets)
        {
            if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp])
            {
                timestamp_counter_set = cs;
                break;
            }
        }

        if (!timestamp_counter_set)
        {
            return;
        }

        MTLCounterSampleBufferDescriptor* csbd = [[MTLCounterSampleBufferDescriptor alloc] init];
        csbd.counterSet = timestamp_counter_set;
        csbd.sampleCount = desc.count;
        csbd.storageMode = MTLStorageModeShared;

        NSError* error = nil;
        counter_sample_buffer = [device newCounterSampleBufferWithDescriptor:csbd error:&error];
        if (error)
        {
            counter_sample_buffer = nil;
        }
    }

    MetalQueryHeap::~MetalQueryHeap()
    {
        if (counter_sample_buffer)
        {
            counter_sample_buffer = nil;
        }
    }

    void* MetalQueryHeap::GetHandle() const
    {
        return (__bridge void*)counter_sample_buffer;
    }
}
