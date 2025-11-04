#pragma once
#include "Graphics/GfxRayTracingPipeline.h"
#import <Metal/Metal.h>
#include <memory>
#include <unordered_set>
#include <string>

@protocol MTLComputePipelineState;

namespace adria
{
    class GfxDevice;

    class MetalRayTracingPipeline : public GfxRayTracingPipeline
    {
    public:
        MetalRayTracingPipeline(GfxDevice* gfx, GfxRayTracingPipelineDesc const& desc);
        virtual ~MetalRayTracingPipeline() override;

        virtual Bool IsValid() const override;
        virtual void* GetNative() const override;
        virtual Bool HasShader(Char const* name) const override;

        id<MTLComputePipelineState> GetRayGenPipeline() const { return raygen_pipeline; }
        id<MTLIntersectionFunctionTable> GetIntersectionTable() const { return intersection_table; }

    private:
        id<MTLComputePipelineState> raygen_pipeline;
        id<MTLIntersectionFunctionTable> intersection_table;
        std::unordered_set<std::string> shader_names;

    private:
        void CacheShaderNames(GfxRayTracingPipelineDesc const& desc);
    };
}
