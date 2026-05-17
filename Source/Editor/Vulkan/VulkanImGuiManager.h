#pragma once
#include "Editor/ImGuiManager.h"
#include "Graphics/Vulkan/VulkanDefines.h"
#include <unordered_map>

namespace adria
{
	class VulkanDevice;
	class GfxDevice;
	class GfxCommandList;

	class VulkanImGuiManager final : public ImGuiManager
	{
	public:
		explicit VulkanImGuiManager(GfxDevice* gfx);
		virtual ~VulkanImGuiManager() override;

		virtual void Begin() const override;
		virtual void End(GfxCommandList* cmd_list) const override;

		virtual void ToggleVisibility() override;
		virtual Bool IsVisible() const override;

		virtual void ShowImage(GfxTexture const& final_texture, ImVec2 image_size) override;
		virtual ImTextureID GetImTextureID(GfxTexture const& texture) override;
		virtual void OnWindowEvent(WindowEventInfo const&) const override;

	private:
		VulkanDevice*    vk_gfx     = nullptr;
		VkDescriptorPool imgui_pool = VK_NULL_HANDLE;
		VkSampler        imgui_sampler = VK_NULL_HANDLE;
		std::string      ini_file;
		Bool             visible    = true;
		std::unordered_map<VkImageView, VkDescriptorSet> texture_cache;
	};
}
