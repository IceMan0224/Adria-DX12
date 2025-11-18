#include "GfxDevice.h"
#include "GfxPipelineState.h"
#if defined(ADRIA_PLATFORM_WINDOWS)
#include "D3D12/D3D12Device.h"
#elif defined(ADRIA_PLATFORM_MACOS)
#include "Metal/MetalDevice.h"
#endif

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

	std::unique_ptr<GfxDevice> CreateGfxDevice(GfxBackend backend, Window* window)
	{
#if defined(ADRIA_PLATFORM_WINDOWS)
		if (backend == GfxBackend::D3D12)
		{
			return std::make_unique<D3D12Device>(window);
		}
#elif defined(ADRIA_PLATFORM_MACOS)
		if (backend == GfxBackend::Metal)
		{
			return std::make_unique<MetalDevice>(window);
		}
#elif defined(ADRIA_PLATFORM_LINUX)
		if (backend == GfxBackend::Vulkan)
		{
			
		}
#endif
		ADRIA_ASSERT_MSG(false, "Requested graphics backend is not supported!");
		return nullptr;
	}

}

