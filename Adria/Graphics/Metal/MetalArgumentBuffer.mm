#import <Metal/Metal.h>
#include "MetalArgumentBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    // Simple descriptor structure for Objective-C + Metal Shader Converter
    // The shader converter expects a buffer of GPU addresses for bindless access
    struct SimpleDescriptorEntry
    {
        uint64_t gpuAddress;      // GPU address for buffers, or resource ID for textures
        uint32_t metadata;         // Additional metadata if needed
        uint32_t padding;          // Alignment
    };

    MetalArgumentBuffer::MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity)
        : metal_gfx(metal_gfx), capacity(initial_capacity), next_free_index(0), descriptor_table_cpu_address(nullptr)
    {
        resource_entries.resize(capacity);
        CreateArgumentBuffer();
    }

    MetalArgumentBuffer::~MetalArgumentBuffer()
    {
        @autoreleasepool
        {
            descriptor_table_buffer = nil;
            descriptor_table_cpu_address = nullptr;
        }
    }

    Uint32 MetalArgumentBuffer::AllocateRange(Uint32 count)
    {
        if (next_free_index + count > capacity)
        {
            Grow();
        }

        Uint32 base_index = next_free_index;
        next_free_index += count;
        return base_index;
    }

    void MetalArgumentBuffer::SetTexture(id<MTLTexture> texture, Uint32 index)
    {
        if (index >= capacity)
        {
            ADRIA_ASSERT(false);
            return;
        }

        if (!descriptor_table_cpu_address)
        {
            ADRIA_ASSERT(false && "Descriptor table not initialized");
            return;
        }

        // For textures with shader converter, we store the GPU resource ID
        // Cast texture to void* to store in descriptor entry - Metal shader converter handles the rest
        SimpleDescriptorEntry* entry = &((SimpleDescriptorEntry*)descriptor_table_cpu_address)[index];
        entry->gpuAddress = (uint64_t)texture;  // Store texture handle
        entry->metadata = 0;  // Texture type marker

        if (index < resource_entries.size())
        {
            resource_entries[index].texture = texture;
            resource_entries[index].type = MetalResourceType::Texture;
        }
    }

    void MetalArgumentBuffer::SetBuffer(id<MTLBuffer> buffer, Uint32 index, Uint64 offset)
    {
        if (index >= capacity)
        {
            ADRIA_ASSERT(false);
            return;
        }

        if (!descriptor_table_cpu_address)
        {
            ADRIA_ASSERT(false && "Descriptor table not initialized");
            return;
        }

        // For buffers, we store the GPU address (not gpuResourceID - that's only for acceleration structures)
        SimpleDescriptorEntry* entry = &((SimpleDescriptorEntry*)descriptor_table_cpu_address)[index];
        entry->gpuAddress = [buffer gpuAddress] + offset;
        entry->metadata = 1;  // Buffer type marker

        if (index < resource_entries.size())
        {
            resource_entries[index].buffer = buffer;
            resource_entries[index].buffer_offset = offset;
            resource_entries[index].type = MetalResourceType::Buffer;
        }
    }

    void MetalArgumentBuffer::SetSampler(id<MTLSamplerState> sampler, Uint32 index)
    {
        if (index >= capacity)
        {
            ADRIA_ASSERT(false);
            return;
        }

        if (!descriptor_table_cpu_address)
        {
            ADRIA_ASSERT(false && "Descriptor table not initialized");
            return;
        }

        // For samplers, store the handle
        SimpleDescriptorEntry* entry = &((SimpleDescriptorEntry*)descriptor_table_cpu_address)[index];
        entry->gpuAddress = (uint64_t)sampler;
        entry->metadata = 2;  // Sampler type marker

        if (index < resource_entries.size())
        {
            resource_entries[index].sampler = sampler;
            resource_entries[index].type = MetalResourceType::Sampler;
        }
    }

    id<MTLTexture> MetalArgumentBuffer::GetTexture(Uint32 index) const
    {
        if (index < resource_entries.size())
        {
            return resource_entries[index].texture;
        }
        return nil;
    }

    id<MTLBuffer> MetalArgumentBuffer::GetBuffer(Uint32 index) const
    {
        if (index < resource_entries.size())
        {
            return resource_entries[index].buffer;
        }
        return nil;
    }

    id<MTLSamplerState> MetalArgumentBuffer::GetSampler(Uint32 index) const
    {
        if (index < resource_entries.size())
        {
            return resource_entries[index].sampler;
        }
        return nil;
    }

    Uint64 MetalArgumentBuffer::GetBufferOffset(Uint32 index) const
    {
        if (index < resource_entries.size())
        {
            return resource_entries[index].buffer_offset;
        }
        return 0;
    }

    MetalResourceType MetalArgumentBuffer::GetResourceType(Uint32 index) const
    {
        if (index < resource_entries.size())
        {
            return resource_entries[index].type;
        }
        return MetalResourceType::Unknown;
    }

    MetalResourceEntry const& MetalArgumentBuffer::GetResourceEntry(Uint32 index) const
    {
        static MetalResourceEntry empty_entry;
        if (index < resource_entries.size())
        {
            return resource_entries[index];
        }
        return empty_entry;
    }

    void MetalArgumentBuffer::Grow()
    {
        Uint32 new_capacity = capacity * 2;
        Uint32 old_capacity = capacity;

        id<MTLDevice> device = metal_gfx->GetMTLDevice();

        // Create new descriptor table buffer
        size_t new_buffer_size = sizeof(SimpleDescriptorEntry) * new_capacity;
        id<MTLBuffer> new_buffer = [device newBufferWithLength:new_buffer_size
                                                        options:MTLResourceStorageModeShared |
                                                                MTLResourceCPUCacheModeWriteCombined |
                                                                MTLResourceHazardTrackingModeTracked];

        SimpleDescriptorEntry* new_cpu_address = (SimpleDescriptorEntry*)[new_buffer contents];

        // Copy existing descriptors to the new buffer
        if (descriptor_table_cpu_address)
        {
            SimpleDescriptorEntry* old_entries = (SimpleDescriptorEntry*)descriptor_table_cpu_address;
            for (Uint32 i = 0; i < old_capacity; ++i)
            {
                MetalResourceEntry const& entry = resource_entries[i];
                switch (entry.type)
                {
                    case MetalResourceType::Texture:
                        if (entry.texture != nil)
                        {
                            new_cpu_address[i].gpuAddress = (uint64_t)entry.texture;
                            new_cpu_address[i].metadata = 0;
                        }
                        break;
                    case MetalResourceType::Buffer:
                        if (entry.buffer != nil)
                        {
                            new_cpu_address[i].gpuAddress = [entry.buffer gpuAddress] + entry.buffer_offset;
                            new_cpu_address[i].metadata = 1;
                        }
                        break;
                    case MetalResourceType::Sampler:
                        if (entry.sampler != nil)
                        {
                            new_cpu_address[i].gpuAddress = (uint64_t)entry.sampler;
                            new_cpu_address[i].metadata = 2;
                        }
                        break;
                    case MetalResourceType::Unknown:
                    default:
                        break;
                }
            }
        }

        // Update to use the new buffer
        descriptor_table_buffer = new_buffer;
        descriptor_table_cpu_address = (IRDescriptorTableEntry*)new_cpu_address;
        capacity = new_capacity;
        resource_entries.resize(new_capacity);

        [new_buffer setLabel:@"BindlessDescriptorTable"];
    }

    void MetalArgumentBuffer::CreateArgumentBuffer()
    {
        id<MTLDevice> device = (id<MTLDevice>)metal_gfx->GetNative();

        // Allocate buffer of simple descriptor entries for Metal Shader Converter
        // This works with Objective-C without needing the Metal-C++ runtime
        size_t buffer_size = sizeof(SimpleDescriptorEntry) * capacity;
        descriptor_table_buffer = [device newBufferWithLength:buffer_size
                                                       options:MTLResourceStorageModeShared |
                                                               MTLResourceCPUCacheModeWriteCombined |
                                                               MTLResourceHazardTrackingModeTracked];

        descriptor_table_cpu_address = (IRDescriptorTableEntry*)[descriptor_table_buffer contents];

        // Initialize all entries to zero
        memset(descriptor_table_cpu_address, 0, buffer_size);

        [descriptor_table_buffer setLabel:@"BindlessDescriptorTable"];
    }
}
