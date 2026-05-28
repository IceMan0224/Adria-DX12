#if defined(ADRIA_PLATFORM_MACOS)
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#endif
#include "VulkanImGuiManager.h"
#include "Graphics/Vulkan/VulkanDevice.h"
#include "Graphics/Vulkan/VulkanCommandList.h"
#include "Graphics/Vulkan/VulkanTexture.h"
#include "Graphics/Vulkan/VulkanCommandQueue.h"
#include "Graphics/Vulkan/VulkanConversions.h"
#include "Graphics/GfxCommandQueue.h"
#include "Platform/Window.h"
#include "Core/Paths.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#if defined(ADRIA_PLATFORM_WINDOWS)
#include "imgui_impl_win32.h"
#elif defined(ADRIA_PLATFORM_MACOS)
#include "imgui_impl_osx.h"
#endif
#include "implot.h"
#include "IconsFontAwesome6.h"
#include "Logging/Log.h"

#if defined(ADRIA_PLATFORM_WINDOWS)
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace adria
{
	ADRIA_LOG_CHANNEL(Editor);

	VulkanImGuiManager::VulkanImGuiManager(GfxDevice* gfx)
		: vk_gfx(static_cast<VulkanDevice*>(gfx))
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		ini_file = paths::IniDir + "imgui.ini";
		io.IniFilename = ini_file.c_str();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigWindowsResizeFromEdges = true;
		io.ConfigViewportsNoTaskBarIcon = true;

		ImFontConfig font_config{};
		std::string font_path = paths::FontsDir + "ComicMono/ComicMono.ttf";
		io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &font_config);
		font_config.MergeMode = true;
		ImWchar const icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		std::string icon_path = paths::FontsDir + "FontAwesome/" FONT_ICON_FILE_NAME_FAS;
		io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 15.0f, &font_config, icon_ranges);

#if defined(ADRIA_PLATFORM_WINDOWS)
		ImGui_ImplWin32_Init(vk_gfx->GetWindowHandle());
#elif defined(ADRIA_PLATFORM_MACOS)
		CAMetalLayer* metal_layer = (__bridge CAMetalLayer*)vk_gfx->GetWindowHandle();
		NSView* content_view = nil;
		if ([metal_layer.delegate isKindOfClass:[NSView class]])
		{
			NSView* delegate_view = (NSView*)metal_layer.delegate;
			content_view = [delegate_view.window contentView];
		}
		if (!content_view)
		{
			NSWindow* ns_window = [NSApp mainWindow];
			if (!ns_window) ns_window = [[NSApp windows] firstObject];
			content_view = [ns_window contentView];
		}
		ImGui_ImplOSX_Init(content_view);
#endif

		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 }
		};
		VkDescriptorPoolCreateInfo pool_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_ci.maxSets       = 256;
		pool_ci.poolSizeCount = 1;
		pool_ci.pPoolSizes    = pool_sizes;
		VK_CHECK(vkCreateDescriptorPool(vk_gfx->GetVkDevice(), &pool_ci, nullptr, &imgui_pool));

		VkSamplerCreateInfo sampler_ci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		sampler_ci.magFilter    = VK_FILTER_LINEAR;
		sampler_ci.minFilter    = VK_FILTER_LINEAR;
		sampler_ci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_ci.maxLod       = VK_LOD_CLAMP_NONE;
		VK_CHECK(vkCreateSampler(vk_gfx->GetVkDevice(), &sampler_ci, nullptr, &imgui_sampler));

		ImGui_ImplVulkan_InitInfo init_info{};
		init_info.ApiVersion     = VK_API_VERSION_1_3;
		init_info.Instance       = vk_gfx->GetVkInstance();
		init_info.PhysicalDevice = vk_gfx->GetVkPhysicalDevice();
		init_info.Device         = vk_gfx->GetVkDevice();
		init_info.QueueFamily    = vk_gfx->GetQueueFamilyIndex(GfxCommandListType::Graphics);
		init_info.Queue          = static_cast<VulkanCommandQueue*>(vk_gfx->GetGraphicsCommandQueue())->GetQueue();
		init_info.DescriptorPool = imgui_pool;
		init_info.MinImageCount  = GFX_BACKBUFFER_COUNT;
		init_info.ImageCount     = GFX_BACKBUFFER_COUNT;
		init_info.UseDynamicRendering = true;

		VkFormat swapchain_fmt = ConvertFormat(vk_gfx->GetBackbuffer()->GetFormat());
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount   = 1;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_fmt;

		ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_PlatformHasViewports;
		ImGui_ImplVulkan_Init(&init_info);
	}

	VulkanImGuiManager::~VulkanImGuiManager()
	{
		vk_gfx->WaitForGPU();
		texture_cache.clear();
		ImGui_ImplVulkan_Shutdown();
#if defined(ADRIA_PLATFORM_WINDOWS)
		ImGui_ImplWin32_Shutdown();
#elif defined(ADRIA_PLATFORM_MACOS)
		ImGui_ImplOSX_Shutdown();
#endif
		if (imgui_sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(vk_gfx->GetVkDevice(), imgui_sampler, nullptr);
		}
		if (imgui_pool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(vk_gfx->GetVkDevice(), imgui_pool, nullptr);
		}
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void VulkanImGuiManager::Begin() const
	{
		ImGui_ImplVulkan_NewFrame();
#if defined(ADRIA_PLATFORM_WINDOWS)
		ImGui_ImplWin32_NewFrame();
#elif defined(ADRIA_PLATFORM_MACOS)
		NSWindow* ns_window = [NSApp mainWindow];
		if (!ns_window) 
		{
			ns_window = [[NSApp windows] firstObject];
		}
		NSView* content_view = [ns_window contentView];
		ImGui_ImplOSX_NewFrame(content_view);
#endif

		GfxTexture* backbuffer = vk_gfx->GetBackbuffer();
		ImGuiIO& io = ImGui::GetIO();
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
		io.DisplaySize = ImVec2((Float)backbuffer->GetWidth(), (Float)backbuffer->GetHeight());

		ImGui::NewFrame();
	}

	void VulkanImGuiManager::End(GfxCommandList* cmd_list) const
	{
		ImGui::Render();

		if (visible)
		{
			VulkanCommandList* vk_cmd = static_cast<VulkanCommandList*>(cmd_list);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_cmd->GetCommandBuffer());
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void VulkanImGuiManager::ToggleVisibility()
	{
		visible = !visible;
	}

	Bool VulkanImGuiManager::IsVisible() const
	{
		return visible;
	}

	void VulkanImGuiManager::ShowImage(GfxTexture const& texture, ImVec2 image_size)
	{
		ImTextureID tex_id = GetImTextureID(texture);
		if (tex_id != (ImTextureID)0)
		{
			ImGui::Image(tex_id, image_size);
		}
	}

	ImTextureID VulkanImGuiManager::GetImTextureID(GfxTexture const& texture)
	{
		VulkanTexture const& vk_tex = static_cast<VulkanTexture const&>(texture);
		VkImageView view = vk_tex.GetDefaultSRVView();
		if (view == VK_NULL_HANDLE) return (ImTextureID)0;

		auto it = texture_cache.find(&texture);
		if (it != texture_cache.end())
		{
			return (ImTextureID)it->second;
		}

		VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(imgui_sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		texture_cache[&texture] = ds;
		return (ImTextureID)ds;
	}

	void VulkanImGuiManager::OnWindowEvent(WindowEventInfo const& info) const
	{
#if defined(ADRIA_PLATFORM_WINDOWS)
		ImGui_ImplWin32_WndProcHandler((HWND)info.handle, info.msg, (WPARAM)info.wparam, (LPARAM)info.lparam);
#endif
	}
}
