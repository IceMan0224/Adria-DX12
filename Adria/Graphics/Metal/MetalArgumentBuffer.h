#pragma once
#import <Metal/Metal.h>
#include "MetalDescriptor.h"

@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLArgumentEncoder;

namespace adria
{
    class MetalDevice;

    enum class MetalResourceType : Uint8
    {
        Texture,
        Buffer,
        Sampler,
        Unknown
    };

    struct MetalResourceEntry
    {
        id<MTLTexture> texture;
        id<MTLBuffer> buffer;
        id<MTLSamplerState> sampler;
        Uint64 buffer_offset;
        MetalResourceType type;

        MetalResourceEntry() : texture(nil), buffer(nil), sampler(nil), buffer_offset(0), type(MetalResourceType::Unknown) {}
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
        void SetSampler(id<MTLSamplerState> sampler, Uint32 index);

        id<MTLTexture> GetTexture(Uint32 index) const;
        id<MTLBuffer> GetBuffer(Uint32 index) const;
        id<MTLSamplerState> GetSampler(Uint32 index) const;
        Uint64 GetBufferOffset(Uint32 index) const;

        MetalResourceType GetResourceType(Uint32 index) const;
        MetalResourceEntry const& GetResourceEntry(Uint32 index) const;

    private:
        MetalDevice* metal_gfx;
        id<MTLBuffer> argument_buffer;
        id<MTLArgumentEncoder> argument_encoder;
        Uint32 capacity;
        Uint32 next_free_index;
        std::vector<MetalResourceEntry> resource_entries; 

    private:
        void Grow();
        void CreateArgumentBuffer();
    };
}
