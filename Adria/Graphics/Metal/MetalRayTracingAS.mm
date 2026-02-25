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

    static MTLAccelerationStructureUsage ConvertASFlags(GfxRayTracingASFlags flags)
    {
        MTLAccelerationStructureUsage usage = MTLAccelerationStructureUsageNone;

        if (flags & GfxRayTracingASFlag_AllowUpdate)
        {
            usage |= MTLAccelerationStructureUsageRefit;
        }

        if (flags & GfxRayTracingASFlag_PreferFastBuild)
        {
            usage |= MTLAccelerationStructureUsagePreferFastBuild;
        }

        return usage;
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

        accel_descriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
        accel_descriptor.geometryDescriptors = geometryDescriptors;
        accel_descriptor.usage = ConvertASFlags(flags);

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accel_descriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::None;
        result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        acceleration_structure = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
        metal_gfx->MakeResident(acceleration_structure);

        scratch_buffer = [device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    }

    MetalRayTracingBLAS::~MetalRayTracingBLAS()
    {
        @autoreleasepool
        {
            accel_descriptor = nil;
            scratch_buffer = nil;
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
        instance_buffer_desc.size = sizeof(MTLAccelerationStructureUserIDInstanceDescriptor) * instances.size();
        instance_buffer_desc.resource_usage = GfxResourceUsage::Default;
        instance_buffer_desc.bind_flags = GfxBindFlag::None;
        instance_buffer = gfx->CreateBuffer(instance_buffer_desc);

        accel_descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
        accel_descriptor.instancedAccelerationStructures = [NSMutableArray array];
        accel_descriptor.usage = ConvertASFlags(flags);

        NSMutableDictionary* blasToIndexMap = [NSMutableDictionary dictionary];
        for (auto const& inst : instances)
        {
            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.blas);
            id<MTLAccelerationStructure> blas_as = blas->GetAccelerationStructure();

            if (![accel_descriptor.instancedAccelerationStructures containsObject:blas_as])
            {
                NSUInteger index = [accel_descriptor.instancedAccelerationStructures count];
                [(NSMutableArray*)accel_descriptor.instancedAccelerationStructures addObject:blas_as];
                [blasToIndexMap setObject:@(index) forKey:[NSValue valueWithPointer:blas]];
                blas_list.push_back(blas_as);
            }
        }

        MetalBuffer* metal_instance_buffer = static_cast<MetalBuffer*>(instance_buffer.get());
        MTLAccelerationStructureUserIDInstanceDescriptor* instanceData =
            (MTLAccelerationStructureUserIDInstanceDescriptor*)[metal_instance_buffer->GetMetalBuffer() contents];

        for (Uint32 i = 0; i < instances.size(); ++i)
        {
            auto const& inst = instances[i];
            MTLAccelerationStructureUserIDInstanceDescriptor& mtl_inst = instanceData[i];

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

            mtl_inst.userID = inst.instance_id;
            MetalRayTracingBLAS* blas = static_cast<MetalRayTracingBLAS*>(inst.blas);
            NSNumber* index = [blasToIndexMap objectForKey:[NSValue valueWithPointer:blas]];
            mtl_inst.accelerationStructureIndex = [index unsignedIntValue];
        }

        accel_descriptor.instanceCount = instances.size();
        accel_descriptor.instanceDescriptorBuffer = metal_instance_buffer->GetMetalBuffer();
        accel_descriptor.instanceDescriptorBufferOffset = 0;
        accel_descriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeUserID;

        MTLAccelerationStructureSizes sizes = [device accelerationStructureSizesWithDescriptor:accel_descriptor];

        GfxBufferDesc result_buffer_desc{};
        result_buffer_desc.size = sizes.accelerationStructureSize;
        result_buffer_desc.resource_usage = GfxResourceUsage::Default;
        result_buffer_desc.bind_flags = GfxBindFlag::None;
        result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
        result_buffer = gfx->CreateBuffer(result_buffer_desc);

        acceleration_structure = [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
        metal_gfx->MakeResident(acceleration_structure);

        scratch_buffer = [device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];

        Usize header_size = sizeof(IRRaytracingAccelerationStructureGPUHeader);
        Usize instance_contributions_size = sizeof(Uint32) * instance_count;
        Usize total_size = header_size + instance_contributions_size;

        GfxBufferDesc gpu_header_desc{};
        gpu_header_desc.size = total_size;
        gpu_header_desc.resource_usage = GfxResourceUsage::Upload;  
        gpu_header_desc.bind_flags = GfxBindFlag::None;
        gpu_header_buffer = gfx->CreateBuffer(gpu_header_desc);
        MetalBuffer* metal_gpu_header = static_cast<MetalBuffer*>(gpu_header_buffer.get());

        metal_gfx->MakeResident(metal_gpu_header->GetMetalBuffer());
        metal_gfx->MakeResident(metal_instance_buffer->GetMetalBuffer());

        Uint8* header_data = static_cast<Uint8*>([metal_gpu_header->GetMetalBuffer() contents]);
        std::vector<Uint32> instance_contributions(instance_count, 0);

        Uint8* instance_contributions_buffer = header_data + header_size;
        Uint64 gpu_header_gpu_address = metal_gpu_header->GetGpuAddress();
        Uint64 instance_contributions_gpu_address = gpu_header_gpu_address + header_size;
        MTLResourceID as_resource_id = acceleration_structure.gpuResourceID;

        ADRIA_ASSERT(header_data != nullptr && "GPU header buffer contents is null");
        ADRIA_ASSERT(as_resource_id._impl != 0 && "AS GPU resource ID is invalid");
        ADRIA_ASSERT(acceleration_structure != nil && "Acceleration structure is nil");

        IRRaytracingSetAccelerationStructure(
            header_data,
            as_resource_id,
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
            accel_descriptor = nil;
            scratch_buffer = nil;
            acceleration_structure = nil;
        }
    }

    Uint64 MetalRayTracingTLAS::GetGpuAddress() const
    {
        return acceleration_structure.gpuResourceID._impl;
    }
}
