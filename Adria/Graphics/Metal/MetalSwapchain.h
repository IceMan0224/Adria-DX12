#pragma once
#include "Graphics/GfxSwapchain.h"
#include "Graphics/GfxTexture.h"
#include <memory>

@class CAMetalLayer;
@protocol MTLCommandQueue;

namespace adria
{
    class MetalSwapchain : public GfxSwapchain
    {
    public:
        MetalSwapchain(GfxDevice* gfx, void* window_handle, Uint32 width, Uint32 height);
        virtual ~MetalSwapchain() override;

        virtual void* GetNative() const override { return nullptr; }
        virtual void Present() override;
        virtual void Resize(Uint32 width, Uint32 height) override;

        CAMetalLayer* GetMetalLayer() const { return metal_layer; }
        id GetCurrentDrawable() const;

    private:
        CAMetalLayer* metal_layer;
        id current_drawable;
        GfxDevice* gfx;
        Uint32 width;
        Uint32 height;
    };
}
