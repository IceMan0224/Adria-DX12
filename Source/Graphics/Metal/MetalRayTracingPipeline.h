#pragma once
#include "Graphics/GfxRayTracingPipeline.h"
#import <Metal/Metal.h>
#include <memory>
#include <unordered_set>
#include <unordered_map>
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
        id<MTLVisibleFunctionTable> GetVisibleFunctionTable() const { return visible_function_table; }
        Uint32 GetShaderFunctionIndex(Char const* name) const;

    private:
        id<MTLComputePipelineState> raygen_pipeline;
        id<MTLIntersectionFunctionTable> intersection_table;
        id<MTLVisibleFunctionTable> visible_function_table;
        std::unordered_set<std::string> shader_names;
        std::unordered_map<std::string, Uint32> shader_to_index;

    private:
        void CacheShaderNames(GfxRayTracingPipelineDesc const& desc);
    };
}
