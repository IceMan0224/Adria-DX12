#pragma once
#import <Metal/Metal.h>
#include "Graphics/GfxRayTracingAS.h"
#include <memory>

@protocol MTLAccelerationStructure;

namespace adria
{
    class GfxBuffer;
    class MetalCommandList;

    class MetalRayTracingBLAS : public GfxRayTracingBLAS
    {
        friend class MetalCommandList;
    public:
        MetalRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
        virtual ~MetalRayTracingBLAS() override;

        virtual Uint64 GetGpuAddress() const override;
        virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

        id<MTLAccelerationStructure> GetAccelerationStructure() const { return acceleration_structure; }

    private:
        std::unique_ptr<GfxBuffer> result_buffer;
        id<MTLAccelerationStructure> acceleration_structure;
        MTLPrimitiveAccelerationStructureDescriptor* accel_descriptor;
        id<MTLBuffer> scratch_buffer;
    };

    class MetalRayTracingTLAS : public GfxRayTracingTLAS
    {
        friend class MetalCommandList;
    public:
        MetalRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
        virtual ~MetalRayTracingTLAS() override;

        virtual Uint64 GetGpuAddress() const override;
        virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }
        virtual GfxBuffer const* GetGpuHeaderBuffer() const override { return gpu_header_buffer.get(); }
        virtual void UpdateInstances(std::span<GfxRayTracingInstance> instances) override;

        id<MTLAccelerationStructure> GetAccelerationStructure() const { return acceleration_structure; }
        std::vector<id<MTLAccelerationStructure>> const& GetBLASList() const { return blas_list; }

    private:
        std::unique_ptr<GfxBuffer> result_buffer;
        std::unique_ptr<GfxBuffer> instance_buffer;
        std::unique_ptr<GfxBuffer> gpu_header_buffer;
        id<MTLAccelerationStructure> acceleration_structure;
        std::vector<id<MTLAccelerationStructure>> blas_list;
        Uint32 instance_count;
        MTLInstanceAccelerationStructureDescriptor* accel_descriptor;
        id<MTLBuffer> scratch_buffer;
    };
}
