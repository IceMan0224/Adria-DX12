#pragma once
#include "Graphics/GfxSwapchain.h"
#include "Graphics/GfxTexture.h"
#include <memory>

@class CAMetalLayer;
@protocol MTLCommandQueue;
@protocol CAMetalDrawable;

namespace adria
{
    class MetalTexture;

    class MetalSwapchain : public GfxSwapchain
    {
    public:
        MetalSwapchain(GfxDevice* gfx, void* window_handle, Uint32 width, Uint32 height);
        virtual ~MetalSwapchain() override;

        virtual void SetAsRenderTarget(GfxCommandList* cmd_list) override {}
        virtual void ClearBackbuffer(GfxCommandList* cmd_list) override {}
        virtual Bool Present(Bool vsync) override;
        virtual void OnResize(Uint32 w, Uint32 h) override;
        virtual Uint32 GetBackbufferIndex() const override { return frame_index; }
        virtual GfxTexture* GetBackbuffer() const override;

        CAMetalLayer* GetMetalLayer() const { return metal_layer; }
        id<CAMetalDrawable> GetCurrentDrawable();

    private:
        CAMetalLayer* metal_layer;
        mutable id<CAMetalDrawable> current_drawable;
        GfxDevice* gfx;
        Uint32 width;
        Uint32 height;
        Uint32 frame_index = 0;

        static constexpr Uint32 BACKBUFFER_COUNT = 2;
        std::unique_ptr<MetalTexture> back_buffers[BACKBUFFER_COUNT];
    };
}
