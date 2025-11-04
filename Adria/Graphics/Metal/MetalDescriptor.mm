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
}
