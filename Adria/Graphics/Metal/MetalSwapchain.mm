#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalSwapchain.h"
#include "MetalDevice.h"

namespace adria
{
    MetalSwapchain::MetalSwapchain(GfxDevice* gfx, void* window_handle, Uint32 w, Uint32 h)
        : gfx(gfx), width(w), height(h), current_drawable(nil)
    {
        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            NSView* contentView = [nsWindow contentView];

            metal_layer = [CAMetalLayer layer];
            metal_layer.device = (id<MTLDevice>)gfx->GetNative();
            metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            metal_layer.framebufferOnly = NO;
            metal_layer.drawableSize = CGSizeMake(w, h);

            [contentView setLayer:metal_layer];
            [contentView setWantsLayer:YES];
        }
    }

    MetalSwapchain::~MetalSwapchain()
    {
        @autoreleasepool
        {
            current_drawable = nil;
            metal_layer = nil;
        }
    }

    void MetalSwapchain::Present()
    {
        current_drawable = nil;
    }

    void MetalSwapchain::Resize(Uint32 w, Uint32 h)
    {
        width = w;
        height = h;
        metal_layer.drawableSize = CGSizeMake(w, h);
    }

    id MetalSwapchain::GetCurrentDrawable() const
    {
        if (!current_drawable)
        {
            current_drawable = [metal_layer nextDrawable];
        }
        return current_drawable;
    }
}
