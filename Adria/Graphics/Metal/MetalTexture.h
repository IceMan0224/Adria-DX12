#pragma once
#include "Graphics/GfxTexture.h"

@protocol MTLTexture;

namespace adria
{
    class MetalTexture final : public GfxTexture
    {
    public:
        MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc);
        MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data);
        MetalTexture(GfxDevice* gfx, void* metal_texture, GfxTextureDesc const& desc); // For backbuffer
        virtual ~MetalTexture() override;

        virtual void* GetNative() const override;
        virtual void* GetSharedHandle() const override;
        virtual void SetName(Char const* name) override;

        id<MTLTexture> GetMetalTexture() const { return metal_texture; }

    private:
        id<MTLTexture> metal_texture;
        Bool owns_texture = true;
    };
}
