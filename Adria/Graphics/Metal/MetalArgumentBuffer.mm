#import <Metal/Metal.h>
#include "MetalArgumentBuffer.h"
#include "MetalDevice.h"

namespace adria
{
    MetalArgumentBuffer::MetalArgumentBuffer(MetalDevice* metal_gfx, Uint32 initial_capacity)
        : metal_gfx(metal_gfx), capacity(initial_capacity), next_free_index(0)
    {
        resource_types.resize(capacity, MetalResourceType::Unknown);
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

        if (index < resource_types.size())
        {
            resource_types[index] = MetalResourceType::Texture;
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

        if (index < resource_types.size())
        {
            resource_types[index] = MetalResourceType::Buffer;
        }
    }

    MetalResourceType MetalArgumentBuffer::GetResourceType(Uint32 index) const
    {
        if (index < resource_types.size())
        {
            return resource_types[index];
        }
        return MetalResourceType::Unknown;
    }

    void MetalArgumentBuffer::Grow()
    {
        Uint32 new_capacity = capacity * 2;

        id<MTLDevice> device = metal_gfx->GetMTLDevice();

        NSMutableArray<MTLArgumentDescriptor*>* arguments = [NSMutableArray array];
        for (Uint32 i = 0; i < new_capacity; ++i)
        {
            MTLArgumentDescriptor* arg = [MTLArgumentDescriptor argumentDescriptor];
            arg.index = i;
            arg.dataType = MTLDataTypePointer; 
            arg.access = MTLArgumentAccessReadWrite;
            ADRIA_TODO("textureType if its a texture argument?");
            [arguments addObject:arg];
        }

        id<MTLArgumentEncoder> new_encoder = [device newArgumentEncoderWithArguments:arguments];
        NSUInteger encoded_length = new_encoder.encodedLength;
        id<MTLBuffer> new_buffer = [device newBufferWithLength:encoded_length
                                                        options:MTLResourceStorageModeShared];

        argument_buffer = new_buffer;
        argument_encoder = new_encoder;
        capacity = new_capacity;
        resource_types.resize(capacity, MetalResourceType::Unknown);
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
