#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxSwapchain.h"

namespace adria
{
	class VulkanDevice;

	class VulkanSwapchain final : public GfxSwapchain
	{
	public:
		VulkanSwapchain(VulkanDevice* gfx, VkSurfaceKHR surface, Uint32 width, Uint32 height);
		virtual ~VulkanSwapchain() override;

		virtual Bool Present(Bool vsync) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;

		virtual Uint32       GetBackbufferIndex() const override { return image_index; }
		virtual GfxTexture*  GetBackbuffer()      const override { return back_buffers[image_index].get(); }

		VkFormat             GetFormat()    const { return surface_format.format; }
		VkSemaphore          GetImageAvailableSemaphore() const { return image_available_semaphores[current_semaphore]; }
		VkSemaphore          GetRenderFinishedSemaphore() const { return render_finished_semaphores[current_semaphore]; }

		Bool AcquireNextImage();

	private:
		VulkanDevice*   gfx           = nullptr;
		VkSurfaceKHR    surface        = VK_NULL_HANDLE;
		VkSwapchainKHR  swapchain      = VK_NULL_HANDLE;

		VkSurfaceFormatKHR surface_format{};
		VkExtent2D         extent{};
		Uint32             image_index   = 0;
		Uint32             current_semaphore = 0;

		std::vector<VkImage>                     swapchain_images;
		std::vector<std::unique_ptr<GfxTexture>> back_buffers;

		VkSemaphore image_available_semaphores[GFX_BACKBUFFER_COUNT]{};
		VkSemaphore render_finished_semaphores[GFX_BACKBUFFER_COUNT]{};

	private:
		void CreateSwapchain(Uint32 w, Uint32 h);
		void DestroySwapchain();
		void CreateBackbuffers();
	};
}
