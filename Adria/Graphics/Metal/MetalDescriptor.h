#pragma once
#include "Graphics/GfxDescriptor.h"

namespace adria
{
    class MetalArgumentBuffer;

    struct MetalDescriptor
    {
        MetalArgumentBuffer* parent_buffer = nullptr;
        Uint32 index = static_cast<Uint32>(-1);

        void Increment(Uint32 multiply = 1) { index += multiply; }
        Bool operator==(MetalDescriptor const& other) const
        {
            return parent_buffer == other.parent_buffer && index == other.index;
        }
        Bool IsValid() const
        {
            return parent_buffer != nullptr;
        }
    };

    GfxDescriptor   EncodeFromMetalDescriptor(MetalDescriptor const& internal_desc);
    MetalDescriptor DecodeToMetalDescriptor(GfxDescriptor const& desc);
}
