#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalDevice.h"
#include "Platform/Window.h"

namespace adria
{
    MetalDevice::MetalDevice(Window* _window) : window(_window)
    {
        // Basic Metal device initialization
        // For now, just a stub - will be implemented later when we add rendering
    }

    MetalDevice::~MetalDevice()
    {
    }

    void MetalDevice::OnResize(Uint32 w, Uint32 h)
    {
        // Handle window resize
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
        // Return a dummy fence reference - this should never be used in stub
        static GfxFence* static_dummy_fence = nullptr;
        return *static_dummy_fence; // Will crash if actually used, which is fine for stub
    }

    GfxShadingRateInfo const& MetalDevice::GetShadingRateInfo() const
    {
        return shading_rate_info;
    }
}
