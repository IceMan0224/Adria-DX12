#include "GfxDevice.h"
#include "GfxPipelineState.h"

namespace adria
{
	std::unique_ptr<GfxGraphicsPipelineState> GfxDevice::CreateManagedGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc)
	{
		return std::make_unique<GfxGraphicsPipelineState>(this, desc);
	}

	std::unique_ptr<GfxComputePipelineState> GfxDevice::CreateManagedComputePipelineState(GfxComputePipelineStateDesc const& desc)
	{
		return std::make_unique<GfxComputePipelineState>(this, desc);
	}

	std::unique_ptr<GfxMeshShaderPipelineState> GfxDevice::CreateManagedMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc)
	{
		return std::make_unique<GfxMeshShaderPipelineState>(this, desc);
	}
}

