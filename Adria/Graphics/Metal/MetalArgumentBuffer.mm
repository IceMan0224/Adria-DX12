#import <Metal/Metal.h>
#include "MetalArgumentBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    MetalArgumentBuffer::MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity)
        : metal_gfx(metal_gfx), capacity(initial_capacity), next_free_index(0)
    {
        resource_entries.resize(capacity);
        CreateArgumentBuffer();
    }

    MetalArgumentBuffer::~MetalArgumentBuffer()
    {
        @autoreleasepool
        {
            argument_buffer = nil;
            argument_encoder = nil;
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

        [argument_encoder setArgumentBuffer:argument_buffer offset:0];
        [argument_encoder setTexture:texture atIndex:index];

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

        [argument_encoder setArgumentBuffer:argument_buffer offset:0];
        [argument_encoder setBuffer:buffer offset:offset atIndex:index];

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

        [argument_encoder setArgumentBuffer:argument_buffer offset:0];
        [argument_encoder setSamplerState:sampler atIndex:index];

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

        NSMutableArray<MTLArgumentDescriptor*>* arguments = [NSMutableArray array];
        for (Uint32 i = 0; i < new_capacity; ++i)
        {
            MTLArgumentDescriptor* arg = [MTLArgumentDescriptor argumentDescriptor];
            arg.index = i;
            arg.dataType = MTLDataTypePointer;
            arg.access = MTLArgumentAccessReadWrite;
            [arguments addObject:arg];
        }

        // Create new encoder and buffer
        id<MTLArgumentEncoder> new_encoder = [device newArgumentEncoderWithArguments:arguments];
        NSUInteger encoded_length = new_encoder.encodedLength;
        id<MTLBuffer> new_buffer = [device newBufferWithLength:encoded_length
                                                        options:MTLResourceStorageModeShared];

        // Set the new encoder to use the new buffer
        [new_encoder setArgumentBuffer:new_buffer offset:0];

        // Copy existing resources to the new buffer
        for (Uint32 i = 0; i < old_capacity; ++i)
        {
            MetalResourceEntry const& entry = resource_entries[i];
            switch (entry.type)
            {
                case MetalResourceType::Texture:
                    if (entry.texture != nil)
                    {
                        [new_encoder setTexture:entry.texture atIndex:i];
                    }
                    break;
                case MetalResourceType::Buffer:
                    if (entry.buffer != nil)
                    {
                        [new_encoder setBuffer:entry.buffer offset:entry.buffer_offset atIndex:i];
                    }
                    break;
                case MetalResourceType::Sampler:
                    if (entry.sampler != nil)
                    {
                        [new_encoder setSamplerState:entry.sampler atIndex:i];
                    }
                    break;
                case MetalResourceType::Unknown:
                default:
                    break;
            }
        }

        // Update to use the new buffer and encoder
        argument_buffer = new_buffer;
        argument_encoder = new_encoder;
        capacity = new_capacity;
        resource_entries.resize(new_capacity);

        [new_buffer setLabel:@"BindlessArgumentBuffer"];
    }

    void MetalArgumentBuffer::CreateArgumentBuffer()
    {
        id<MTLDevice> device = (id<MTLDevice>)metal_gfx->GetNative();

        NSMutableArray<MTLArgumentDescriptor*>* arguments = [NSMutableArray array];
        for (Uint32 i = 0; i < capacity; ++i)
        {
            MTLArgumentDescriptor* arg = [MTLArgumentDescriptor argumentDescriptor];
            arg.index = i;
            arg.dataType = MTLDataTypePointer; 
            arg.access = MTLArgumentAccessReadWrite; 
            [arguments addObject:arg];
        }
        argument_encoder = [device newArgumentEncoderWithArguments:arguments];
        NSUInteger encoded_length = argument_encoder.encodedLength;
        argument_buffer = [device newBufferWithLength:encoded_length
                                              options:MTLResourceStorageModeShared];

        [argument_buffer setLabel:@"BindlessArgumentBuffer"];
    }
}
