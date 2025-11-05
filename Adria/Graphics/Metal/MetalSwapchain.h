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

        virtual void SetAsRenderTarget(GfxCommandList* cmd_list) override {}
        virtual void ClearBackbuffer(GfxCommandList* cmd_list) override {}
        virtual Bool Present(Bool vsync) override;
        virtual void OnResize(Uint32 w, Uint32 h) override;
        virtual Uint32 GetBackbufferIndex() const override { return 0; }
        virtual GfxTexture* GetBackbuffer() const override { return nullptr; }

        CAMetalLayer* GetMetalLayer() const { return metal_layer; }
        id GetCurrentDrawable() const;

    private:
        CAMetalLayer* metal_layer;
        mutable id current_drawable;
        GfxDevice* gfx;
        Uint32 width;
        Uint32 height;
    };
}
