#import <Metal/Metal.h>
#import <Metal/MTLResource.h>
#include "MetalArgumentBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    struct DescriptorEntry
    {
        uint64_t resourceID;    // gpuResourceID for textures/buffers, handle for samplers
        uint32_t offset;       
        uint32_t flags;        
        
        static constexpr uint32_t TYPE_MASK = 0x3;
        static constexpr uint32_t TYPE_TEXTURE = 0;
        static constexpr uint32_t TYPE_BUFFER = 1;
        static constexpr uint32_t TYPE_SAMPLER = 2;
    };
    
    static_assert(sizeof(DescriptorEntry) == 16, "DescriptorEntry must be 16 bytes for GPU alignment");

    MetalArgumentBuffer::MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity)
        : metal_gfx(metal_gfx)
        , capacity(initial_capacity)
        , next_free_index(0)
        , descriptor_cpu_ptr(nullptr)
    {
        ADRIA_ASSERT(metal_gfx != nullptr);
        ADRIA_ASSERT(initial_capacity > 0);
        
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

    void MetalArgumentBuffer::SetTexture(id<MTLTexture> texture, Uint32 index)
    {
        if (!ValidateIndex(index)) return;
        
        DescriptorEntry* entry = GetDescriptorEntry(index);
        if (texture != nil)
        {
            // Store the texture pointer directly
            entry->resourceID = reinterpret_cast<uint64_t>((__bridge void*)texture);
            entry->offset = 0;
            entry->flags = DescriptorEntry::TYPE_TEXTURE;
            
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
        
        DescriptorEntry* entry = GetDescriptorEntry(index);
        if (buffer != nil)
        {
            // Use GPU address for buffers - this is the actual GPU memory address
            entry->resourceID = buffer.gpuAddress + offset;
            entry->offset = static_cast<uint32_t>(offset);
            entry->flags = DescriptorEntry::TYPE_BUFFER;
            
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
        
        DescriptorEntry* entry = GetDescriptorEntry(index);
        if (sampler != nil)
        {
            // Store the sampler pointer directly
            entry->resourceID = reinterpret_cast<uint64_t>((__bridge void*)sampler);
            entry->offset = 0;
            entry->flags = DescriptorEntry::TYPE_SAMPLER;
            
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

    void MetalArgumentBuffer::MakeResourcesResident(id<MTLRenderCommandEncoder> encoder, MTLRenderStages stages)
    {
        if (!encoder) 
        {
            return;
        }

        for (Uint32 i = 0; i < next_free_index; ++i)
        {
            const MetalResourceEntry& entry = resource_entries[i];
            
            switch (entry.type)
            {
                case MetalResourceType::Texture:
                    if (entry.texture)
                    {
                        [encoder useResource:entry.texture 
                                        usage:MTLResourceUsageRead 
                                        stages:stages];
                    }
                    break;
                    
                case MetalResourceType::Buffer:
                    if (entry.buffer)
                    {
                        [encoder useResource:entry.buffer 
                                        usage:MTLResourceUsageRead 
                                        stages:stages];
                    }
                    break;
                    
                case MetalResourceType::Sampler:
                case MetalResourceType::Unknown:
                default:
                    break;
            }
        }
    }

    void MetalArgumentBuffer::MakeResourcesResident(id<MTLComputeCommandEncoder> encoder)
    {
        if (!encoder) return;

        for (Uint32 i = 0; i < next_free_index; ++i)
        {
            const MetalResourceEntry& entry = resource_entries[i];
            
            switch (entry.type)
            {
                case MetalResourceType::Texture:
                    if (entry.texture)
                    {
                        [encoder useResource:entry.texture usage:MTLResourceUsageRead];
                    }
                    break;
                    
                case MetalResourceType::Buffer:
                    if (entry.buffer)
                    {
                        [encoder useResource:entry.buffer usage:MTLResourceUsageRead];
                    }
                    break;
                    
                case MetalResourceType::Sampler:
                case MetalResourceType::Unknown:
                default:
                    break;
            }
        }
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
            
            descriptor_cpu_ptr = static_cast<DescriptorEntry*>([descriptor_buffer contents]);
            std::memset(descriptor_cpu_ptr, 0, buffer_size);
            
            [descriptor_buffer setLabel:@"BindlessDescriptorTable"];
        }
    }

    void MetalArgumentBuffer::GrowCapacity(Uint32 min_capacity)
    {
        // Grow to next power of 2 or min_capacity, whichever is larger
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
            
            DescriptorEntry* new_cpu_ptr = static_cast<DescriptorEntry*>([new_buffer contents]);
            
            // Copy existing descriptors
            if (descriptor_cpu_ptr)
            {
                std::memcpy(new_cpu_ptr, descriptor_cpu_ptr, sizeof(DescriptorEntry) * capacity);
            }
            
            // Zero new entries
            std::memset(new_cpu_ptr + capacity, 0, sizeof(DescriptorEntry) * (new_capacity - capacity));
            
            // Update state
            descriptor_buffer = new_buffer;
            descriptor_cpu_ptr = new_cpu_ptr;
            capacity = new_capacity;
            resource_entries.resize(new_capacity);
            
            [new_buffer setLabel:@"BindlessDescriptorTable"];
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

    DescriptorEntry* MetalArgumentBuffer::GetDescriptorEntry(Uint32 index)
    {
        return &descriptor_cpu_ptr[index];
    }

    void MetalArgumentBuffer::ClearEntry(Uint32 index)
    {
        if (index < resource_entries.size())
        {
            resource_entries[index] = MetalResourceEntry{};
        }
        
        if (descriptor_cpu_ptr)
        {
            descriptor_cpu_ptr[index] = DescriptorEntry{};
        }
    }
}
