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
        virtual ~MetalGraphicsPipelineState() override;

        virtual void* GetNative() const override;
        virtual GfxPipelineStateType GetType() const override { return GfxPipelineStateType::Graphics; }

        id<MTLRenderPipelineState> GetPipelineState() const { return pipeline_state; }
        id<MTLDepthStencilState> GetDepthStencilState() const { return depth_stencil_state; }

        GfxPrimitiveTopologyType GetTopologyType() const { return topology_type; }
        GfxCullMode GetCullMode() const { return cull_mode; }

    private:
        id<MTLRenderPipelineState> pipeline_state;
        id<MTLDepthStencilState> depth_stencil_state;
        GfxPrimitiveTopologyType topology_type;
        GfxCullMode cull_mode;
    };

    class MetalComputePipelineState final : public GfxPipelineState
    {
    public:
        MetalComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc);
        virtual ~MetalComputePipelineState() override;

        virtual void* GetNative() const override;
        virtual GfxPipelineStateType GetType() const override { return GfxPipelineStateType::Compute; }

        id<MTLComputePipelineState> GetPipelineState() const { return pipeline_state; }

    private:
        id<MTLComputePipelineState> pipeline_state;
    };
}
