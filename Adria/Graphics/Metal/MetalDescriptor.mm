#include "MetalDescriptor.h"
#include "MetalArgumentBuffer.h"

namespace adria
{
    static GfxDescriptor EncodeFromMetalDescriptor(MetalArgumentBuffer* buffer, Uint32 index)
    {
        GfxDescriptor desc{};
        desc.opaque_data[0] = reinterpret_cast<Uint64>(buffer);
        desc.opaque_data[1] = static_cast<Uint64>(index);
        return desc;
    }

    GfxDescriptor EncodeFromMetalDescriptor(MetalDescriptor const& internal_desc)
    {
        return EncodeFromMetalDescriptor(internal_desc.parent_buffer, internal_desc.index);
    }

    MetalDescriptor DecodeToMetalDescriptor(GfxDescriptor const& desc)
    {
        MetalDescriptor internal_desc{};
        internal_desc.parent_buffer = reinterpret_cast<MetalArgumentBuffer*>(desc.opaque_data[0]);
        internal_desc.index = static_cast<Uint32>(desc.opaque_data[1]);
        return internal_desc;
    }

    GfxDescriptor EncodeFromMetalRenderTargetDescriptor(MetalRenderTargetDescriptor const& rt_desc)
    {
        GfxDescriptor desc{};
        desc.opaque_data[0] = reinterpret_cast<Uint64>((__bridge void*)rt_desc.texture);
        desc.opaque_data[1] = (static_cast<Uint64>(rt_desc.mip_level) << 32) | static_cast<Uint64>(rt_desc.array_slice);
        return desc;
    }

    MetalRenderTargetDescriptor DecodeToMetalRenderTargetDescriptor(GfxDescriptor const& desc)
    {
        MetalRenderTargetDescriptor rt_desc{};
        rt_desc.texture = (__bridge id<MTLTexture>)reinterpret_cast<void*>(desc.opaque_data[0]);
        rt_desc.mip_level = static_cast<Uint32>(desc.opaque_data[1] >> 32);
        rt_desc.array_slice = static_cast<Uint32>(desc.opaque_data[1] & 0xFFFFFFFF);
        return rt_desc;
    }
}
