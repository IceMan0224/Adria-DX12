#import <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "MetalRayTracingAS.h"
#include "MetalDevice.h"
#include "MetalBuffer.h"
#include "MetalConversions.h"
#include "Utilities/Ref.h"
#include "Utilities/Enum.h"

namespace adria
{
    static MTLAccelerationStructureInstanceOptions ConvertInstanceFlags(GfxRayTracingInstanceFlags flags)
    {
        MTLAccelerationStructureInstanceOptions options = MTLAccelerationStructureInstanceOptionNone;
        if (flags & GfxRayTracingInstanceFlag_ForceOpaque)
        {
            options |= MTLAccelerationStructureInstanceOptionOpaque;
        }
        if (flags & GfxRayTracingInstanceFlag_ForceNoOpaque)
        {
            options |= MTLAccelerationStructureInstanceOptionNonOpaque;
        }
        if (flags & GfxRayTracingInstanceFlag_CullDisable)
        {
            options |= MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
        }
        return options;
    }

    MetalRayTracingBLAS::MetalRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
    {
        MetalDevice* metal_gfx = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_gfx->GetMTLDevice();

        NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>* geometryDescriptors = [NSMutableArray array];
        for (auto const& geom : geometries)
        {
            MTLAccelerationStructureTriangleGeometryDescriptor* triangleGeometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];

            MetalBuffer* vertex_buffer = static_cast<MetalBuffer*>(geom.vertex_buffer);
            triangleGeometry.vertexBuffer = vertex_buffer->GetMetalBuffer();
            triangleGeometry.vertexBufferOffset = geom.vertex_buffer_offset;
            triangleGeometry.vertexStride = geom.vertex_stride;
            triangleGeometry.vertexFormat = MTLAttributeFormatFloat3;

            if (geom.index_buffer)
            {
                MetalBuffer* index_buffer = static_cast<MetalBuffer*>(geom.index_buffer);
                triangleGeometry.indexBuffer = index_buffer->GetMetalBuffer();
                triangleGeometry.indexBufferOffset = geom.index_buffer_offset;
                triangleGeometry.indexType = (geom.index_format == GfxFormat::R16_UINT) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
                triangleGeometry.triangleCount = geom.index_count / 3;
            }
            else
            {
                triangleGeometry.triangleCount = geom.vertex_count / 3;
            }

            triangleGeometry.opaque = geom.opaque ? YES : NO;
            [geometryDescriptors addObject:triangleGeometry];
        }

        MTLPrimitiveAccelerationStructureDescriptor* accelDescriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        accelDescriptor.geometryDescriptors = geometryDescriptors;

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accelDescriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::None;
        result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        GfxBufferDesc scratch_buffer_desc{};
        scratch_buffer_desc.size = sizes.buildScratchBufferSize;
        scratch_buffer_desc.resource_usage = GfxResourceUsage::Default;
        scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
        scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);

        MetalBuffer* metal_result_buffer = static_cast<MetalBuffer*>(result_buffer.get());
        acceleration_structure = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];

        id<MTLCommandQueue> commandQueue = metal_gfx->GetMTLCommandQueue();
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLAccelerationStructureCommandEncoder> accelEncoder = [commandBuffer accelerationStructureCommandEncoder];

        MetalBuffer* metal_scratch_buffer = static_cast<MetalBuffer*>(scratch_buffer.get());
        [accelEncoder buildAccelerationStructure:acceleration_structure
                                      descriptor:accelDescriptor
                                   scratchBuffer:metal_scratch_buffer->GetMetalBuffer()
                             scratchBufferOffset:0];

        [accelEncoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
    }

    MetalRayTracingBLAS::~MetalRayTracingBLAS()
    {
        @autoreleasepool
        {
            acceleration_structure = nil;
        }
    }

    Uint64 MetalRayTracingBLAS::GetGpuAddress() const
    {
        return acceleration_structure.gpuResourceID._impl;
    }

    MetalRayTracingTLAS::MetalRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
        : instance_count(static_cast<Uint32>(instances.size()))
    {
        MetalDevice* metal_gfx = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_gfx->GetMTLDevice();

        GfxBufferDesc instance_buffer_desc{};
        instance_buffer_desc.size = sizeof(MTLAccelerationStructureInstanceDescriptor) * instances.size();
        instance_buffer_desc.resource_usage = GfxResourceUsage::Default;
        instance_buffer_desc.bind_flags = GfxBindFlag::None;
        instance_buffer = gfx->CreateBuffer(instance_buffer_desc);

        MTLInstanceAccelerationStructureDescriptor* accelDescriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
        accelDescriptor.instancedAccelerationStructures = [NSMutableArray array];

        NSMutableDictionary* blasToIndexMap = [NSMutableDictionary dictionary];
        for (auto const& inst : instances)
        {
            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.blas);
            id<MTLAccelerationStructure> blas_as = blas->GetAccelerationStructure();

            if (![accelDescriptor.instancedAccelerationStructures containsObject:blas_as])
            {
                NSUInteger index = [accelDescriptor.instancedAccelerationStructures count];
                [(NSMutableArray*)accelDescriptor.instancedAccelerationStructures addObject:blas_as];
                [blasToIndexMap setObject:@(index) forKey:[NSValue valueWithPointer:blas]];
            }
        }

        MetalBuffer* metal_instance_buffer = static_cast<MetalBuffer*>(instance_buffer.get());
        MTLAccelerationStructureInstanceDescriptor* instanceData =
            (MTLAccelerationStructureInstanceDescriptor*)[metal_instance_buffer->GetMetalBuffer() contents];

        for (Uint32 i = 0; i < instances.size(); ++i)
        {
            auto const& inst = instances[i];
            MTLAccelerationStructureInstanceDescriptor& mtl_inst = instanceData[i];

            for (Uint32 row = 0; row < 3; ++row)
            {
                for (Uint32 col = 0; col < 4; ++col)
                {
                    mtl_inst.transformationMatrix.columns[col][row] = inst.transform[row][col];
                }
            }

            mtl_inst.mask = inst.instance_mask;
            mtl_inst.intersectionFunctionTableOffset = 0;

            mtl_inst.options = MTLAccelerationStructureInstanceOptionNone;
            if (inst.flags & GfxRayTracingInstanceFlag_CullDisable)
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
            }
            if (inst.flags & GfxRayTracingInstanceFlag_FrontCCW)
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise;
            }
            if (inst.flags & GfxRayTracingInstanceFlag_ForceOpaque)
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionOpaque;
            }
            if (inst.flags & GfxRayTracingInstanceFlag_ForceNoOpaque)
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionNonOpaque;
            }

            // Set the correct accelerationStructureIndex
            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.blas);
            NSNumber* index = [blasToIndexMap objectForKey:[NSValue valueWithPointer:blas]];
            mtl_inst.accelerationStructureIndex = [index unsignedIntValue];
        }

        accelDescriptor.instanceCount = instances.size();
        accelDescriptor.instanceDescriptorBuffer = metal_instance_buffer->GetMetalBuffer();
        accelDescriptor.instanceDescriptorBufferOffset = 0;
        accelDescriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accelDescriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::None;
        result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        GfxBufferDesc scratch_buffer_desc{};
        scratch_buffer_desc.size = sizes.buildScratchBufferSize;
        scratch_buffer_desc.resource_usage = GfxResourceUsage::Default;
        scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
        scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);

        MetalBuffer* metal_result_buffer = static_cast<MetalBuffer*>(result_buffer.get());
        acceleration_structure = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];

        id<MTLCommandQueue> commandQueue = metal_gfx->GetMTLCommandQueue();
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLAccelerationStructureCommandEncoder> accelEncoder = [commandBuffer accelerationStructureCommandEncoder];

        MetalBuffer* metal_scratch_buffer = static_cast<MetalBuffer*>(scratch_buffer.get());
        [accelEncoder buildAccelerationStructure:acceleration_structure
                                      descriptor:accelDescriptor
                                   scratchBuffer:metal_scratch_buffer->GetMetalBuffer()
                             scratchBufferOffset:0];

        [accelEncoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];

        Usize header_size = sizeof(IRRaytracingAccelerationStructureGPUHeader);
        Usize instance_contributions_size = sizeof(Uint32) * instance_count;
        Usize total_size = header_size + instance_contributions_size;

        GfxBufferDesc gpu_header_desc{};
        gpu_header_desc.size = total_size;
        gpu_header_desc.resource_usage = GfxResourceUsage::Upload;  
        gpu_header_desc.bind_flags = GfxBindFlag::None;
        gpu_header_buffer = gfx->CreateBuffer(gpu_header_desc);
        MetalBuffer* metal_gpu_header = static_cast<MetalBuffer*>(gpu_header_buffer.get());
        Uint8* header_data = static_cast<Uint8*>([metal_gpu_header->GetMetalBuffer() contents]);
        std::vector<Uint32> instance_contributions(instance_count, 0);

        Uint8* instance_contributions_buffer = header_data + header_size;
        Uint64 instance_contributions_gpu_address = metal_gpu_header->GetMetalBuffer().gpuAddress + header_size;

        IRRaytracingSetAccelerationStructure(
            header_data,                              
            acceleration_structure.gpuResourceID,     
            instance_contributions_buffer,            
            instance_contributions_gpu_address,       
            instance_contributions.data(),
            instance_count                            
        );
    }

    MetalRayTracingTLAS::~MetalRayTracingTLAS()
    {
        @autoreleasepool
        {
            acceleration_structure = nil;
        }
    }

    Uint64 MetalRayTracingTLAS::GetGpuAddress() const
    {
        return acceleration_structure.gpuResourceID._impl;
    }
}
