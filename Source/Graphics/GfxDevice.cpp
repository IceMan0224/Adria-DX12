#include "GfxDevice.h"
#include "GfxPipelineState.h"
#include "Core/CommandLineOptions.h"
#if defined(ADRIA_PLATFORM_WINDOWS)
#include "D3D12/D3D12Device.h"
#endif
#if defined(ADRIA_PLATFORM_MACOS)
#include "Metal/MetalDevice.h"
#endif
#if defined(ADRIA_HAS_VULKAN_SDK)
#include "Vulkan/VulkanDevice.h"
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
#endif
#if defined(ADRIA_PLATFORM_MACOS)
		if (backend == GfxBackend::Metal)
		{
			return std::make_unique<MetalDevice>(window);
		}
#endif
#if defined(ADRIA_HAS_VULKAN_SDK)
		if (backend == GfxBackend::Vulkan)
		{
			return std::make_unique<VulkanDevice>(window);
		}
#endif
		ADRIA_ASSERT_MSG(false, "Requested graphics backend is not supported!");
		return nullptr;
	}

	std::unique_ptr<GfxDevice> CreateGfxDevice(Window* window)
	{
		std::string const& backend_str = CommandLineOptions::GetGfxBackend();
		if (backend_str == "vulkan" || backend_str == "vk")
		{
			return CreateGfxDevice(GfxBackend::Vulkan, window);
		}
		if (backend_str == "metal" || backend_str == "mtl")
		{
			return CreateGfxDevice(GfxBackend::Metal, window);
		}
		if (backend_str == "d3d12" || backend_str == "dx12")
		{
			return CreateGfxDevice(GfxBackend::D3D12, window);
		}

#if defined(ADRIA_PLATFORM_WINDOWS)
		return CreateGfxDevice(GfxBackend::D3D12, window);
#elif defined(ADRIA_PLATFORM_MACOS)
		return CreateGfxDevice(GfxBackend::Metal, window);
#elif defined(ADRIA_PLATFORM_LINUX)
		return CreateGfxDevice(GfxBackend::Vulkan, window);
#endif
	}
}
