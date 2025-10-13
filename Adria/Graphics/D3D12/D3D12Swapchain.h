#pragma once
#include "Graphics/GfxSwapchain.h"

namespace adria
{
    class D3D12Swapchain final : public GfxSwapchain
    {
    public:
		D3D12Swapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc);
        ~D3D12Swapchain();

        virtual void SetAsRenderTarget(GfxCommandList* cmd_list) override;
		virtual void ClearBackbuffer(GfxCommandList* cmd_list) override;
		virtual Bool Present(Bool vsync) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;

		virtual Uint32 GetBackbufferIndex() const override { return backbuffer_index; }
		virtual GfxTexture* GetBackbuffer() const override { return back_buffers[backbuffer_index].get(); }

    private:
        GfxDevice* gfx = nullptr;
		Ref<IDXGISwapChain4>				swapchain = nullptr;
		std::unique_ptr<GfxTexture>			back_buffers[GFX_BACKBUFFER_COUNT] = { nullptr };
		GfxDescriptor					    backbuffer_rtvs[GFX_BACKBUFFER_COUNT];
		Uint32		 width;
		Uint32		 height;
		Uint32		 backbuffer_index;

    private:
		void CreateBackbuffers();
		GfxDescriptor GetBackbufferDescriptor() const;
    };
}