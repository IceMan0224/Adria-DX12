#pragma once
#import <Metal/Metal.h>
#include "MetalDescriptor.h"
#include "Utilities/RingOffsetAllocator.h"
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

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

    //typedef struct IRDescriptorTableEntry
    //{
    //    uint64_t gpuVA;
    //    uint64_t textureViewID;
    //    uint64_t metadata;
    //} IRDescriptorTableEntry;

    class MetalArgumentBuffer final
    {
    public:
        MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity, Uint32 reserve_size = 0);
        ADRIA_NONCOPYABLE_NONMOVABLE(MetalArgumentBuffer)
        ~MetalArgumentBuffer();

        ADRIA_FORCEINLINE Uint32 GetCapacity() const { return capacity; }
        ADRIA_FORCEINLINE Uint32 GetNextFreeIndex() const { return next_free_index; }
        ADRIA_FORCEINLINE id<MTLBuffer> GetBuffer() const { return descriptor_buffer; }
        ADRIA_FORCEINLINE Uint32 GetReservedSize() const { return reserved_size; }

        [[deprecated("Use AllocatePersistent or AllocateTransient instead")]]
        Uint32 AllocateRange(Uint32 count);

        Uint32 AllocatePersistent(Uint32 count);
        Uint32 AllocateTransient(Uint32 count);

        void SetResourceAtIndex(id<MTLTexture> texture, Uint32 index);
        void SetResourceAtIndex(id<MTLBuffer> buffer, Uint32 index, Uint64 offset = 0);

        void FinishCurrentFrame(Uint64 frame);
        void ReleaseCompletedFrames(Uint64 completed_frame);

        void SetTexture(id<MTLTexture> texture, Uint32 index);
        void SetBuffer(id<MTLBuffer> buffer, Uint32 index, Uint64 offset = 0);
        void SetSampler(id<MTLSamplerState> sampler, Uint32 index);

        id<MTLTexture> GetTexture(Uint32 index) const;
        id<MTLBuffer> GetBuffer(Uint32 index) const;
        id<MTLSamplerState> GetSampler(Uint32 index) const;
        Uint64 GetBufferOffset(Uint32 index) const;
        MetalResourceType GetResourceType(Uint32 index) const;
        IRDescriptorTableEntry const& GetResourceEntry(Uint32 index) const;

    private:
        MetalDevice* metal_gfx;
        id<MTLBuffer> descriptor_buffer;
        void* descriptor_cpu_ptr;
        Uint32 capacity;
        Uint32 next_free_index;
        std::vector<IRDescriptorTableEntry> resource_entries;

        RingOffsetAllocator ring_allocator;
        Uint32 reserved_size;
        Uint32 next_persistent_index;

        id<MTLTexture> default_texture_2d;
        id<MTLTexture> default_texture_3d;
        id<MTLBuffer> default_buffer;
        id<MTLSamplerState> default_sampler;

    private:
        void CreateDescriptorBuffer();
        void InitializeDefaultResources();
        void SetDefaultTexture(Uint32 index);
        void SetDefaultBuffer(Uint32 index);
        void SetDefaultSampler(Uint32 index);
        void GrowCapacity(Uint32 min_capacity);
        Bool ValidateIndex(Uint32 index) const;
        void* GetDescriptorEntry(Uint32 index);
        void ClearEntry(Uint32 index);
    };
}
