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
        virtual Uint64 GetGpuAddress() const override;
        virtual void* Map() override;
        virtual void Unmap() override;
        virtual void* GetSharedHandle() const override;
        virtual Uint32 GetRowPitch(Uint32 mip_level = 0) const override;
        virtual void SetName(Char const* name) override;

        void UpdateHandle(void* metal_texture_handle); // For updating backbuffer texture each frame

        id<MTLTexture> GetMetalTexture() const { return metal_texture; }

    private:
        id<MTLTexture> metal_texture;
        Bool owns_texture = true;
    };
}
