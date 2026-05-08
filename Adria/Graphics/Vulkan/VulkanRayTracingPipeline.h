#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxRayTracingPipeline.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace adria
{
	class GfxDevice;
	class VulkanDevice;

	class VulkanRayTracingPipeline final : public GfxRayTracingPipeline
	{
	public:
		VulkanRayTracingPipeline(GfxDevice* gfx, GfxRayTracingPipelineDesc const& desc);
		virtual ~VulkanRayTracingPipeline() override;

		virtual Bool  IsValid()                   const override { return pipeline != VK_NULL_HANDLE; }
		virtual void* GetNative()                 const override { return (void*)pipeline; }
		virtual Bool  HasShader(Char const* name) const override;

		VkPipeline GetVkPipeline() const { return pipeline; }

		void const* GetShaderGroupHandle(Char const* name) const;
		Uint32 GetShaderGroupHandleSize() const { return handle_size; }
		Uint32 GetShaderGroupHandleAlignment() const { return handle_alignment; }
		Uint32 GetShaderGroupBaseAlignment() const { return base_alignment; }

	private:
		VulkanDevice* vk_device = nullptr;
		VkPipeline    pipeline  = VK_NULL_HANDLE;
		std::vector<VkShaderModule> modules;

		std::vector<Uint8> handle_storage;
		std::unordered_map<std::string, Uint32> group_index_by_name;
		Uint32 handle_size      = 0;
		Uint32 handle_alignment = 0;
		Uint32 base_alignment   = 0;
	};
}
