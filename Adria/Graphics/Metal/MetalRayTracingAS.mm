#import <Metal/Metal.h>
#include "MetalRayTracingAS.h"
#include "MetalDevice.h"
#include "MetalBuffer.h"
#include "MetalConversions.h"
#include "Utilities/Ref.h"

namespace adria
{
    static MTLAccelerationStructureInstanceOptions ConvertGeometryFlags(GfxRayTracingGeometryFlags flags)
    {
        MTLAccelerationStructureInstanceOptions options = MTLAccelerationStructureInstanceOptionNone;

        if (HasAnyFlag(flags, GfxRayTracingGeometryFlag::Opaque))
        {
            options |= MTLAccelerationStructureInstanceOptionOpaque;
        }
        if (HasAnyFlag(flags, GfxRayTracingGeometryFlag::NoDuplicateAnyHitInvocation))
        {
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
            triangleGeometry.vertexBufferOffset = geom.vertex_offset;
            triangleGeometry.vertexStride = geom.vertex_stride;
            triangleGeometry.vertexFormat = MTLAttributeFormatFloat3; 

            if (geom.index_buffer)
            {
                MetalBuffer* index_buffer = static_cast<MetalBuffer*>(geom.index_buffer);
                triangleGeometry.indexBuffer = index_buffer->GetMetalBuffer();
                triangleGeometry.indexBufferOffset = geom.index_offset;
                triangleGeometry.indexType = (geom.index_format == GfxFormat::R16_UINT) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
                triangleGeometry.triangleCount = geom.index_count / 3;
            }
            else
            {
                triangleGeometry.triangleCount = geom.vertex_count / 3;
            }

            if (HasAnyFlag(geom.flags, GfxRayTracingGeometryFlag::Opaque))
            {
                triangleGeometry.opaque = YES;
            }
            else
            {
                triangleGeometry.opaque = NO;
            }

            [geometryDescriptors addObject:triangleGeometry];
        }

        MTLPrimitiveAccelerationStructureDescriptor* accelDescriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        accelDescriptor.geometryDescriptors = geometryDescriptors;

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accelDescriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::AccelerationStructure;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        GfxBufferDesc scratch_buffer_desc{};
        scratch_buffer_desc.size = sizes.buildScratchBufferSize;
        scratch_buffer_desc.resource_usage = GfxResourceUsage::Default;
        scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
        scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);

        MetalBuffer* metal_result_buffer = static_cast<MetalBuffer*>(result_buffer.get());
        acceleration_structure = [device newAccelerationStructureWithBuffer:metal_result_buffer->GetMetalBuffer()
                                                                      offset:0];

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
    {
        MetalDevice* metal_gfx = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_gfx->GetMTLDevice();

        GfxBufferDesc instance_buffer_desc{};
        instance_buffer_desc.size = sizeof(MTLAccelerationStructureInstanceDescriptor) * instances.size();
        instance_buffer_desc.resource_usage = GfxResourceUsage::Default;
        instance_buffer_desc.bind_flags = GfxBindFlag::None;
        instance_buffer = gfx->CreateBuffer(instance_buffer_desc);

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

            mtl_inst.instanceID = inst.instance_id;
            mtl_inst.mask = inst.instance_mask;
            mtl_inst.intersectionFunctionTableOffset = inst.instance_contribution_to_hit_group_index;

            mtl_inst.options = MTLAccelerationStructureInstanceOptionNone;
            if (HasAnyFlag(inst.flags, GfxRayTracingInstanceFlag::TriangleFacingCullDisable))
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
            }
            if (HasAnyFlag(inst.flags, GfxRayTracingInstanceFlag::TriangleFrontCounterClockwise))
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise;
            }
            if (HasAnyFlag(inst.flags, GfxRayTracingInstanceFlag::ForceOpaque))
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionOpaque;
            }
            if (HasAnyFlag(inst.flags, GfxRayTracingInstanceFlag::ForceNonOpaque))
            {
                mtl_inst.options |= MTLAccelerationStructureInstanceOptionNonOpaque;
            }
            mtl_inst.accelerationStructureIndex = 0; 

            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.bottom_level_as);
            id<MTLAccelerationStructure> blas_as = blas->GetAccelerationStructure();
        }

        MTLInstanceAccelerationStructureDescriptor* accelDescriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
        accelDescriptor.instancedAccelerationStructures = [NSMutableArray array];

        for (auto const& inst : instances)
        {
            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.bottom_level_as);
            id<MTLAccelerationStructure> blas_as = blas->GetAccelerationStructure();

            if (![accelDescriptor.instancedAccelerationStructures containsObject:blas_as])
            {
                [(NSMutableArray*)accelDescriptor.instancedAccelerationStructures addObject:blas_as];
            }
        }

        accelDescriptor.instanceCount = instances.size();
        accelDescriptor.instanceDescriptorBuffer = metal_instance_buffer->GetMetalBuffer();
        accelDescriptor.instanceDescriptorBufferOffset = 0;
        accelDescriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accelDescriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::AccelerationStructure;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        GfxBufferDesc scratch_buffer_desc{};
        scratch_buffer_desc.size = sizes.buildScratchBufferSize;
        scratch_buffer_desc.resource_usage = GfxResourceUsage::Default;
        scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
        scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);

        MetalBuffer* metal_result_buffer = static_cast<MetalBuffer*>(result_buffer.get());
        acceleration_structure = [device newAccelerationStructureWithBuffer:metal_result_buffer->GetMetalBuffer()
                                                                      offset:0];

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
