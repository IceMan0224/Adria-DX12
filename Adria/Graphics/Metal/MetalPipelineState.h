#pragma once
#include "Graphics/GfxPipelineState.h"

@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;
@protocol MTLDepthStencilState;

namespace adria
{
    class MetalGraphicsPipelineState final : public GfxPipelineState
    {
    public:
        MetalGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc);

#ifdef __OBJC__
        MetalGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc,
                                   id<MTLFunction> vs, id<MTLFunction> ps);
#endif

        virtual ~MetalGraphicsPipelineState() override;

        virtual void* GetNative() const override;
        virtual GfxPipelineStateType GetType() const override { return GfxPipelineStateType::Graphics; }

        id<MTLRenderPipelineState> GetPipelineState() const { return pipeline_state; }
        id<MTLDepthStencilState> GetDepthStencilState() const { return depth_stencil_state; }

        GfxPrimitiveTopologyType GetTopologyType() const { return topology_type; }
        GfxCullMode GetCullMode() const { return cull_mode; }
        Bool GetFrontCounterClockwise() const { return front_counter_clockwise; }
        Float GetDepthBias() const { return depth_bias; }
        Float GetSlopeScaledDepthBias() const { return slope_scaled_depth_bias; }
        Float GetDepthBiasClamp() const { return depth_bias_clamp; }

    private:
        id<MTLRenderPipelineState> pipeline_state;
        id<MTLDepthStencilState> depth_stencil_state;
        GfxPrimitiveTopologyType topology_type;
        GfxCullMode cull_mode;
        Bool front_counter_clockwise;
        Float depth_bias;
        Float slope_scaled_depth_bias;
        Float depth_bias_clamp;
    };

    class MetalMeshShadingPipelineState final : public GfxPipelineState
    {
    public:
        MetalMeshShadingPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc);
        virtual ~MetalMeshShadingPipelineState() override;

        virtual void* GetNative() const override;
        virtual GfxPipelineStateType GetType() const override { return GfxPipelineStateType::MeshShader; }

        id<MTLRenderPipelineState> GetPipelineState() const { return pipeline_state; }
        id<MTLDepthStencilState> GetDepthStencilState() const { return depth_stencil_state; }
        MTLSize GetThreadsPerObjectThreadgroup() const { return threads_per_object_threadgroup; }
        MTLSize GetThreadsPerMeshThreadgroup() const { return threads_per_mesh_threadgroup; }

        GfxCullMode GetCullMode() const { return cull_mode; }
        Bool GetFrontCounterClockwise() const { return front_counter_clockwise; }
        Float GetDepthBias() const { return depth_bias; }
        Float GetSlopeScaledDepthBias() const { return slope_scaled_depth_bias; }
        Float GetDepthBiasClamp() const { return depth_bias_clamp; }

    private:
        id<MTLRenderPipelineState> pipeline_state;
        id<MTLDepthStencilState> depth_stencil_state;
        MTLSize threads_per_object_threadgroup;
        MTLSize threads_per_mesh_threadgroup;
        GfxCullMode cull_mode;
        Bool front_counter_clockwise;
        Float depth_bias;
        Float slope_scaled_depth_bias;
        Float depth_bias_clamp;
    };

    class MetalComputePipelineState final : public GfxPipelineState
    {
    public:
        MetalComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc);
        virtual ~MetalComputePipelineState() override;

        virtual void* GetNative() const override;
        virtual GfxPipelineStateType GetType() const override { return GfxPipelineStateType::Compute; }

        id<MTLComputePipelineState> GetPipelineState() const { return pipeline_state; }
        MTLSize GetThreadsPerThreadgroup() const { return threads_per_threadgroup; }

    private:
        id<MTLComputePipelineState> pipeline_state;
        MTLSize threads_per_threadgroup;
    };
}
