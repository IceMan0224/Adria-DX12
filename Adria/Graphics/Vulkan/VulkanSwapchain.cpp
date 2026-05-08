#include "VulkanSwapchain.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanCommandList.h"
#include "VulkanCommandQueue.h"
#include "VulkanConversions.h"
#include "Graphics/GfxDefines.h"
#include "precomp.h"

namespace adria
{
	VulkanSwapchain::VulkanSwapchain(VulkanDevice* gfx, VkSurfaceKHR in_surface, Uint32 width, Uint32 height)
		: gfx(gfx), surface(in_surface)
	{
		CreateSwapchain(width, height);
		VkDevice device = gfx->GetVkDevice();
		VkSemaphoreCreateInfo ci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			VK_CHECK(vkCreateSemaphore(device, &ci, nullptr, &image_available_semaphores[i]));
			VK_CHECK(vkCreateSemaphore(device, &ci, nullptr, &render_finished_semaphores[i]));
		}
	}

	VulkanSwapchain::~VulkanSwapchain()
	{
		VkDevice device = gfx->GetVkDevice();
		back_buffers.clear();
		DestroySwapchain();
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
			vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
		}
		vkDestroySurfaceKHR(gfx->GetVkInstance(), surface, nullptr);
	}

	void VulkanSwapchain::CreateSwapchain(Uint32 w, Uint32 h)
	{
		VkPhysicalDevice physical_device = gfx->GetVkPhysicalDevice();
		VkDevice device = gfx->GetVkDevice();

		Uint32 format_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());
		surface_format = formats[0];
		for (auto const& f : formats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surface_format = f;
				break;
			}
		}

		Uint32 mode_count = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, nullptr);
		std::vector<VkPresentModeKHR> modes(mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, modes.data());
		VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; 
		for (auto m : modes)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR) { present_mode = m; break; }
		}

		VkSurfaceCapabilitiesKHR caps{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);
		extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
		extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);

		Uint32 image_count = std::max(caps.minImageCount + 1, GFX_BACKBUFFER_COUNT);
		if (caps.maxImageCount > 0)
		{
			image_count = std::min(image_count, caps.maxImageCount);
		}

		VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		ci.surface          = surface;
		ci.minImageCount    = image_count;
		ci.imageFormat      = surface_format.format;
		ci.imageColorSpace  = surface_format.colorSpace;
		ci.imageExtent      = extent;
		ci.imageArrayLayers = 1;
		ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.preTransform     = caps.currentTransform;
		ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		ci.presentMode      = present_mode;
		ci.clipped          = VK_TRUE;
		ci.oldSwapchain     = swapchain;

		VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
		VK_CHECK(vkCreateSwapchainKHR(device, &ci, nullptr, &new_swapchain));

		if (swapchain != VK_NULL_HANDLE)
		{
			DestroySwapchain();
		}
		swapchain = new_swapchain;
		CreateBackbuffers();
	}

	void VulkanSwapchain::DestroySwapchain()
	{
		if (swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(gfx->GetVkDevice(), swapchain, nullptr);
			swapchain = VK_NULL_HANDLE;
		}
	}

	void VulkanSwapchain::CreateBackbuffers()
	{
		VkDevice device = gfx->GetVkDevice();

		Uint32 count = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
		swapchain_images.resize(count);
		vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data());

		back_buffers.clear();
		for (Uint32 i = 0; i < count; ++i)
		{
			GfxTextureDesc bb_desc{};
			bb_desc.type        = GfxTextureType_2D;
			bb_desc.width       = extent.width;
			bb_desc.height      = extent.height;
			bb_desc.mip_levels  = 1;
			bb_desc.array_size  = 1;
			bb_desc.sample_count = 1;
			bb_desc.format      = ConvertFormat(surface_format.format);
			bb_desc.bind_flags  = GfxBindFlag::RenderTarget;
			bb_desc.initial_state = GfxResourceState::Present;

			back_buffers.push_back(gfx->CreateBackbufferTexture(bb_desc, swapchain_images[i]));
		}
	}

	void VulkanSwapchain::OnResize(Uint32 w, Uint32 h)
	{
		gfx->WaitForGPU();
		back_buffers.clear();
		CreateSwapchain(w, h);
	}

	Bool VulkanSwapchain::AcquireNextImage()
	{
		current_semaphore = (current_semaphore + 1) % GFX_BACKBUFFER_COUNT;
		VkResult result = vkAcquireNextImageKHR(gfx->GetVkDevice(), swapchain,
			UINT64_MAX, image_available_semaphores[current_semaphore], VK_NULL_HANDLE, &image_index);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			OnResize(extent.width, extent.height);
			return false;
		}

		ADRIA_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return true;
	}

	Bool VulkanSwapchain::Present(Bool vsync)
	{
		VkSemaphore wait_sem = render_finished_semaphores[current_semaphore];

		VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores    = &wait_sem;
		present_info.swapchainCount     = 1;
		present_info.pSwapchains        = &swapchain;
		present_info.pImageIndices      = &image_index;

		VkQueue graphics_queue = static_cast<VulkanCommandQueue*>(gfx->GetGraphicsCommandQueue())->GetQueue();
		VkResult result = vkQueuePresentKHR(graphics_queue, &present_info);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			OnResize(extent.width, extent.height);
		}
		else if (result != VK_SUBOPTIMAL_KHR)
		{
			VK_CHECK(result);
		}

		return true;
	}
}
