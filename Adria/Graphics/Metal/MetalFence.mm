#import <Metal/Metal.h>
#include "MetalFence.h"
#include "MetalDevice.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

    MetalFence::MetalFence()
    {
    }

    MetalFence::~MetalFence()
    {
        @autoreleasepool
        {
            if (shared_event)
            {
                shared_event = nil;
            }
        }
    }

    Bool MetalFence::Create(GfxDevice* gfx, Char const* name)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        shared_event = [device newSharedEvent];
        if (!shared_event)
        {
            ADRIA_LOG(ERROR, "Failed to create Metal shared event for fence: %s", name);
            return false;
        }
        shared_event.signaledValue = 0;

        if (name)
        {
            shared_event.label = [NSString stringWithUTF8String:name];
        }

        return true;
    }

    void MetalFence::Wait(Uint64 value)
    {
        if (!shared_event)
        {
            return;
        }
        [shared_event waitUntilSignaledValue:value timeoutMS:UINT64_MAX];
    }

    void MetalFence::Signal(Uint64 value)
    {
        if (shared_event)
        {
            shared_event.signaledValue = value;
        }
    }

    Bool MetalFence::IsCompleted(Uint64 value)
    {
        if (!shared_event)
        {
            return true;
        }
        return shared_event.signaledValue >= value;
    }

    Uint64 MetalFence::GetCompletedValue() const
    {
        if (!shared_event)
        {
            return 0;
        }
        return shared_event.signaledValue;
    }

    void* MetalFence::GetHandle() const
    {
        return (__bridge void*)shared_event;
    }
}
