#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalSwapchain.h"
#include "MetalDevice.h"
#include "MetalTexture.h"

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

            GfxTextureDesc texture_desc{};
            texture_desc.width = w;
            texture_desc.height = h;
            texture_desc.format = GfxFormat::B8G8R8A8_UNORM;
            texture_desc.bind_flags = GfxBindFlag::RenderTarget;

            for (Uint32 i = 0; i < BACKBUFFER_COUNT; ++i)
            {
                back_buffers[i] = std::make_unique<MetalTexture>(gfx, nullptr, texture_desc);
                back_buffers[i]->SetName("Backbuffer");
            }
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

    Bool MetalSwapchain::Present(Bool vsync)
    {
        current_drawable = nil;
        frame_index = (frame_index + 1) % BACKBUFFER_COUNT;
        return true;
    }

    void MetalSwapchain::OnResize(Uint32 w, Uint32 h)
    {
        width = w;
        height = h;
        metal_layer.drawableSize = CGSizeMake(w, h);
    }

    GfxTexture* MetalSwapchain::GetBackbuffer() const
    {
        return back_buffers[frame_index].get();
    }

    id<CAMetalDrawable> MetalSwapchain::GetCurrentDrawable()
    {
        if (!current_drawable)
        {
            current_drawable = [metal_layer nextDrawable];
            if (current_drawable)
            {
                id<MTLTexture> mtl_texture = current_drawable.texture;
                back_buffers[frame_index]->UpdateHandle((__bridge void*)mtl_texture);
            }
        }
        return current_drawable;
    }
}
