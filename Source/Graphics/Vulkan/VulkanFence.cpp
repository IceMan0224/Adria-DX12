#include "VulkanFence.h"
#include "Graphics/GfxDevice.h"

namespace adria
{
	VulkanFence::~VulkanFence()
	{
		Destroy();
	}

	void VulkanFence::Destroy()
	{
		if (semaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, semaphore, nullptr);
			semaphore = VK_NULL_HANDLE;
		}
	}

	Bool VulkanFence::Create(GfxDevice* gfx, Char const* name)
	{
		device = (VkDevice)gfx->GetNative();
		VkSemaphoreTypeCreateInfo type_info{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
		type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		type_info.initialValue  = 0;
		VkSemaphoreCreateInfo ci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		ci.pNext = &type_info;
		VK_CHECK(vkCreateSemaphore(device, &ci, nullptr, &semaphore));
		if (name)
		{
			VK_OBJECT_SET_NAME(device, semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
		}
		return true;
	}

	void VulkanFence::Wait(Uint64 value)
	{
		VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores    = &semaphore;
		wait_info.pValues        = &value;
		VK_CHECK(vkWaitSemaphores(device, &wait_info, UINT64_MAX));
	}

	void VulkanFence::Signal(Uint64 value)
	{
		VkSemaphoreSignalInfo signal_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
		signal_info.semaphore = semaphore;
		signal_info.value     = value;
		VK_CHECK(vkSignalSemaphore(device, &signal_info));
	}

	Bool VulkanFence::IsCompleted(Uint64 value)
	{
		return GetCompletedValue() >= value;
	}

	Uint64 VulkanFence::GetCompletedValue() const
	{
		Uint64 current = 0;
		vkGetSemaphoreCounterValue(device, semaphore, &current);
		return current;
	}
}
