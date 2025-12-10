#pragma once
#include "Graphics/GfxDescriptor.h"

@protocol MTLTexture;

namespace adria
{
    struct MetalDescriptor
    {
        Uint32 index = static_cast<Uint32>(-1);

        void Increment(Uint32 multiply = 1) { index += multiply; }
        Bool operator==(MetalDescriptor const& other) const
        {
            return index == other.index;
        }
        Bool IsValid() const
        {
            return index != static_cast<Uint32>(-1);
        }
    };

    struct MetalRenderTargetDescriptor
    {
        id<MTLTexture> texture;
        Uint32 mip_level;
        Uint32 array_slice;

        MetalRenderTargetDescriptor()
            : texture(nullptr), mip_level(0), array_slice(0) {}
    };

    GfxDescriptor   EncodeFromMetalDescriptor(MetalDescriptor const& internal_desc);
    MetalDescriptor DecodeToMetalDescriptor(GfxDescriptor const& desc);

    GfxDescriptor EncodeFromMetalRenderTargetDescriptor(MetalRenderTargetDescriptor const& rt_desc);
    MetalRenderTargetDescriptor DecodeToMetalRenderTargetDescriptor(GfxDescriptor const& desc);
}
