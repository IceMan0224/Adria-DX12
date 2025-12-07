#import <Metal/Metal.h>
#import <Metal/MTLResource.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "MetalArgumentBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);
    using DescriptorEntry = IRDescriptorTableEntry;

    MetalArgumentBuffer::MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity, Uint32 reserve_size)
        : metal_gfx(metal_gfx)
        , capacity(initial_capacity)
        , next_free_index(0)
        , descriptor_cpu_ptr(nullptr)
        , ring_allocator(initial_capacity, reserve_size)  // Initialize ring allocator with capacity and reserved region
        , reserved_size(reserve_size)
        , next_persistent_index(0)
    {
        ADRIA_ASSERT(metal_gfx != nullptr);
        ADRIA_ASSERT(initial_capacity > 0);
        ADRIA_ASSERT(reserve_size <= initial_capacity);

        resource_entries.reserve(capacity);
        resource_entries.resize(capacity);
        CreateDescriptorBuffer();
    }

    MetalArgumentBuffer::~MetalArgumentBuffer()
    {
        @autoreleasepool
        {
            descriptor_buffer = nil;
            descriptor_cpu_ptr = nullptr;
        }
    }

    Uint32 MetalArgumentBuffer::AllocateRange(Uint32 count)
    {
        ADRIA_ASSERT(count > 0);

        if (next_free_index + count > capacity)
        {
            GrowCapacity(next_free_index + count);
        }

        Uint32 base_index = next_free_index;
        next_free_index += count;
        return base_index;
    }

    Uint32 MetalArgumentBuffer::AllocatePersistent(Uint32 count)
    {
        ADRIA_ASSERT(count > 0);
        ADRIA_ASSERT_MSG(next_persistent_index + count <= reserved_size,
                         "Out of persistent bindless slots!");

        Uint32 base_index = next_persistent_index;
        next_persistent_index += count;
        return base_index;
    }

    Uint32 MetalArgumentBuffer::AllocateTransient(Uint32 count)
    {
        ADRIA_ASSERT(count > 0);

        if (reserved_size + count > capacity)
        {
            GrowCapacity(reserved_size + count);
        }

        Uint64 offset = ring_allocator.Allocate(count);
        if (offset == INVALID_ALLOC_OFFSET)
        {
            ADRIA_LOG(ERROR, "Transient descriptor ring buffer full! Consider increasing capacity.");
            ADRIA_ASSERT(false && "Ring buffer out of space!");
            return 0;
        }

        return static_cast<Uint32>(offset);
    }

    void MetalArgumentBuffer::FinishCurrentFrame(Uint64 frame)
    {
        ring_allocator.FinishCurrentFrame(frame);
    }

    void MetalArgumentBuffer::ReleaseCompletedFrames(Uint64 completed_frame)
    {
        ring_allocator.ReleaseCompletedFrames(completed_frame);
    }

    void MetalArgumentBuffer::SetTexture(id<MTLTexture> texture, Uint32 index)
    {
        if (!ValidateIndex(index)) return;

        DescriptorEntry* entry = static_cast<DescriptorEntry*>(GetDescriptorEntry(index));
        if (texture != nil)
        {
            IRDescriptorTableSetTexture(entry, texture, 0.0f /* minLODClamp */, 0 /* metadata */);
            resource_entries[index].texture = texture;
            resource_entries[index].type = MetalResourceType::Texture;
        }
        else
        {
            ClearEntry(index);
        }
    }

    void MetalArgumentBuffer::SetBuffer(id<MTLBuffer> buffer, Uint32 index, Uint64 offset)
    {
        if (!ValidateIndex(index)) return;

        DescriptorEntry* entry = static_cast<DescriptorEntry*>(GetDescriptorEntry(index));
        if (buffer != nil)
        {
            Uint64 gpu_va = buffer.gpuAddress + offset;
            IRDescriptorTableSetBuffer(entry, gpu_va, 0 /* metadata */);

            resource_entries[index].buffer = buffer;
            resource_entries[index].buffer_offset = offset;
            resource_entries[index].type = MetalResourceType::Buffer;
        }
        else
        {
            ClearEntry(index);
        }
    }

    void MetalArgumentBuffer::SetSampler(id<MTLSamplerState> sampler, Uint32 index)
    {
        if (!ValidateIndex(index)) return;

        DescriptorEntry* entry = static_cast<DescriptorEntry*>(GetDescriptorEntry(index));
        if (sampler != nil)
        {
            IRDescriptorTableSetSampler(entry, sampler, 0.0f /* lodBias */);

            resource_entries[index].sampler = sampler;
            resource_entries[index].type = MetalResourceType::Sampler;
        }
        else
        {
            ClearEntry(index);
        }
    }


    id<MTLTexture> MetalArgumentBuffer::GetTexture(Uint32 index) const
    {
        return (index < resource_entries.size()) ? resource_entries[index].texture : nil;
    }

    id<MTLBuffer> MetalArgumentBuffer::GetBuffer(Uint32 index) const
    {
        return (index < resource_entries.size()) ? resource_entries[index].buffer : nil;
    }

    id<MTLSamplerState> MetalArgumentBuffer::GetSampler(Uint32 index) const
    {
        return (index < resource_entries.size()) ? resource_entries[index].sampler : nil;
    }

    Uint64 MetalArgumentBuffer::GetBufferOffset(Uint32 index) const
    {
        return (index < resource_entries.size()) ? resource_entries[index].buffer_offset : 0;
    }

    MetalResourceType MetalArgumentBuffer::GetResourceType(Uint32 index) const
    {
        return (index < resource_entries.size()) ? resource_entries[index].type : MetalResourceType::Unknown;
    }

    MetalResourceEntry const& MetalArgumentBuffer::GetResourceEntry(Uint32 index) const
    {
        static const MetalResourceEntry empty_entry{};
        return (index < resource_entries.size()) ? resource_entries[index] : empty_entry;
    }

    // Private methods

    void MetalArgumentBuffer::CreateDescriptorBuffer()
    {
        @autoreleasepool
        {
            id<MTLDevice> device = metal_gfx->GetMTLDevice();

            const size_t buffer_size = sizeof(DescriptorEntry) * capacity;
            descriptor_buffer = [device newBufferWithLength:buffer_size
                                                     options:MTLResourceStorageModeShared |
                                                             MTLResourceCPUCacheModeWriteCombined |
                                                             MTLResourceHazardTrackingModeTracked];

            ADRIA_ASSERT(descriptor_buffer != nil);

            descriptor_cpu_ptr = [descriptor_buffer contents];
            std::memset(descriptor_cpu_ptr, 0, buffer_size);

            [descriptor_buffer setLabel:@"BindlessDescriptorTable"];
            metal_gfx->MakeResident(descriptor_buffer);
        }
    }

    void MetalArgumentBuffer::GrowCapacity(Uint32 min_capacity)
    {
        Uint32 new_capacity = capacity;
        while (new_capacity < min_capacity)
        {
            new_capacity *= 2;
        }

        @autoreleasepool
        {
            id<MTLDevice> device = metal_gfx->GetMTLDevice();

            const size_t new_buffer_size = sizeof(DescriptorEntry) * new_capacity;
            id<MTLBuffer> new_buffer = [device newBufferWithLength:new_buffer_size
                                                           options:MTLResourceStorageModeShared |
                                                                  MTLResourceCPUCacheModeWriteCombined |
                                                                  MTLResourceHazardTrackingModeTracked];

            void* new_cpu_ptr = [new_buffer contents];

            if (descriptor_cpu_ptr)
            {
                std::memcpy(new_cpu_ptr, descriptor_cpu_ptr, sizeof(DescriptorEntry) * capacity);
            }

            DescriptorEntry* new_entries = static_cast<DescriptorEntry*>(new_cpu_ptr);
            std::memset(new_entries + capacity, 0, sizeof(DescriptorEntry) * (new_capacity - capacity));

            id<MTLBuffer> old_buffer = descriptor_buffer;
            descriptor_buffer = new_buffer;
            descriptor_cpu_ptr = new_cpu_ptr;
            capacity = new_capacity;
            resource_entries.resize(new_capacity);

            ring_allocator = RingOffsetAllocator(new_capacity, reserved_size);

            [new_buffer setLabel:@"BindlessDescriptorTable"];
            metal_gfx->MakeResident(new_buffer);
            if (old_buffer)
            {
                metal_gfx->Evict(old_buffer);
            }
        }
    }

    bool MetalArgumentBuffer::ValidateIndex(Uint32 index) const
    {
        if (index >= capacity)
        {
            ADRIA_ASSERT(false && "Index out of bounds");
            return false;
        }
        
        if (!descriptor_cpu_ptr)
        {
            ADRIA_ASSERT(false && "Descriptor buffer not initialized");
            return false;
        }
        
        return true;
    }

    void* MetalArgumentBuffer::GetDescriptorEntry(Uint32 index)
    {
        return static_cast<DescriptorEntry*>(descriptor_cpu_ptr) + index;
    }

    void MetalArgumentBuffer::ClearEntry(Uint32 index)
    {
        if (index < resource_entries.size())
        {
            resource_entries[index] = MetalResourceEntry{};
        }

        if (descriptor_cpu_ptr)
        {
            DescriptorEntry* entries = static_cast<DescriptorEntry*>(descriptor_cpu_ptr);
            entries[index] = DescriptorEntry{};
        }
    }
}
