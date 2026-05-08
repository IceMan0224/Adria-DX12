#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxFence.h"

namespace adria
{
	class VulkanFence final : public GfxFence
	{
	public:
		VulkanFence() = default;
		virtual ~VulkanFence() override;

		virtual Bool Create(GfxDevice* gfx, Char const* name) override;
		virtual void Wait(Uint64 value) override;
		virtual void Signal(Uint64 value) override;
		virtual Bool IsCompleted(Uint64 value) override;
		virtual Uint64 GetCompletedValue() const override;
		virtual void* GetHandle() const override { return (void*)semaphore; }

		VkSemaphore GetSemaphore() const { return semaphore; }

	private:
		VkDevice  device    = VK_NULL_HANDLE;
		VkSemaphore semaphore = VK_NULL_HANDLE;
	};
}
