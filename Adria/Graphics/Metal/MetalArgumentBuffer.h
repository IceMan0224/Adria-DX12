#pragma once
#import <Metal/Metal.h>
#include "MetalDescriptor.h"
#include <vector>

@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLRenderCommandEncoder;
@protocol MTLComputeCommandEncoder;

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
        id<MTLTexture> texture = nil;
        id<MTLBuffer> buffer = nil;
        id<MTLSamplerState> sampler = nil;
        Uint64 buffer_offset = 0;
        MetalResourceType type = MetalResourceType::Unknown;
    };

    class MetalArgumentBuffer final
    {
    public:
        MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity);
        ~MetalArgumentBuffer();

        // Non-copyable, non-movable
        MetalArgumentBuffer(const MetalArgumentBuffer&) = delete;
        MetalArgumentBuffer& operator=(const MetalArgumentBuffer&) = delete;
        MetalArgumentBuffer(MetalArgumentBuffer&&) = delete;
        MetalArgumentBuffer& operator=(MetalArgumentBuffer&&) = delete;

        // Capacity and allocation
        ADRIA_FORCEINLINE Uint32 GetCapacity() const { return capacity; }
        ADRIA_FORCEINLINE Uint32 GetNextFreeIndex() const { return next_free_index; }
        ADRIA_FORCEINLINE id<MTLBuffer> GetBuffer() const { return descriptor_buffer; }

        Uint32 AllocateRange(Uint32 count);

        // Resource setters
        void SetTexture(id<MTLTexture> texture, Uint32 index);
        void SetBuffer(id<MTLBuffer> buffer, Uint32 index, Uint64 offset = 0);
        void SetSampler(id<MTLSamplerState> sampler, Uint32 index);

        id<MTLTexture> GetTexture(Uint32 index) const;
        id<MTLBuffer> GetBuffer(Uint32 index) const;
        id<MTLSamplerState> GetSampler(Uint32 index) const;
        Uint64 GetBufferOffset(Uint32 index) const;
        MetalResourceType GetResourceType(Uint32 index) const;
        MetalResourceEntry const& GetResourceEntry(Uint32 index) const;

        void MakeResourcesResident(id<MTLRenderCommandEncoder> encoder, 
                                   MTLRenderStages stages = MTLRenderStageVertex | MTLRenderStageFragment);
        void MakeResourcesResident(id<MTLComputeCommandEncoder> encoder);

    private:
        MetalDevice* metal_gfx;
        id<MTLBuffer> descriptor_buffer;
        void* descriptor_cpu_ptr;  
        Uint32 capacity;
        Uint32 next_free_index;
        std::vector<MetalResourceEntry> resource_entries;

    private:
        void CreateDescriptorBuffer();
        void GrowCapacity(Uint32 min_capacity);
        Bool ValidateIndex(Uint32 index) const;
        void* GetDescriptorEntry(Uint32 index);
        void ClearEntry(Uint32 index);
    };
}
