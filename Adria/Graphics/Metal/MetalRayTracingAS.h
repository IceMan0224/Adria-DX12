#pragma once
#import <Metal/Metal.h>
#include "Graphics/GfxRayTracingAS.h"
#include <memory>

@protocol MTLAccelerationStructure;

namespace adria
{
    class GfxBuffer;

    class MetalRayTracingBLAS : public GfxRayTracingBLAS
    {
    public:
        MetalRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
        virtual ~MetalRayTracingBLAS() override;

        virtual Uint64 GetGpuAddress() const override;
        virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

        id<MTLAccelerationStructure> GetAccelerationStructure() const { return acceleration_structure; }

    private:
        std::unique_ptr<GfxBuffer> result_buffer;
        std::unique_ptr<GfxBuffer> scratch_buffer;
        id<MTLAccelerationStructure> acceleration_structure;
    };

    class MetalRayTracingTLAS : public GfxRayTracingTLAS
    {
    public:
        MetalRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
        virtual ~MetalRayTracingTLAS() override;

        virtual Uint64 GetGpuAddress() const override;
        virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

        id<MTLAccelerationStructure> GetAccelerationStructure() const { return acceleration_structure; }

    private:
        std::unique_ptr<GfxBuffer> result_buffer;
        std::unique_ptr<GfxBuffer> scratch_buffer;
        std::unique_ptr<GfxBuffer> instance_buffer;
        id<MTLAccelerationStructure> acceleration_structure;
    };
}
