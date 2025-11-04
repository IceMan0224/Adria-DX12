#pragma once
#import <Metal/Metal.h>
#include "MetalDescriptor.h"

@protocol MTLBuffer;
@protocol MTLArgumentEncoder;

namespace adria
{
    class MetalDevice;

    enum class MetalResourceType : Uint8
    {
        Texture,
        Buffer,
        Unknown
    };

    class MetalArgumentBuffer final
    {
    public:
        MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity);
        ~MetalArgumentBuffer();

        ADRIA_FORCEINLINE Uint32 GetCapacity() const { return capacity; }
        ADRIA_FORCEINLINE Uint32 GetNextFreeIndex() const { return next_free_index; }

        id<MTLBuffer> GetBuffer() const { return argument_buffer; }
        id<MTLArgumentEncoder> GetEncoder() const { return argument_encoder; }

        Uint32 AllocateRange(Uint32 count);

        void SetTexture(id<MTLTexture> texture, Uint32 index);
        void SetBuffer(id<MTLBuffer> buffer, Uint32 index, Uint64 offset = 0);

        MetalResourceType GetResourceType(Uint32 index) const;

    private:
        MetalDevice* metal_gfx;
        id<MTLBuffer> argument_buffer;
        id<MTLArgumentEncoder> argument_encoder;
        Uint32 capacity;
        Uint32 next_free_index;
        std::vector<MetalResourceType> resource_types; 

    private:
        void Grow();
        void CreateArgumentBuffer();
    };
}
