#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxPipelineState.h"

namespace adria
{
	class VulkanPipelineState final : public GfxPipelineState
	{
	public:
		VulkanPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc);
		VulkanPipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc);
		VulkanPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc);
		virtual ~VulkanPipelineState() override;

		virtual GfxPipelineStateType GetType() const override  { return type; }
		virtual void* GetNative() const override               { return (void*)pipeline; }

		VkPipeline GetPipeline() const { return pipeline; }

	private:
		VkDevice             device   = VK_NULL_HANDLE;
		VkPipeline           pipeline = VK_NULL_HANDLE;
		GfxPipelineStateType type;
	};
}
