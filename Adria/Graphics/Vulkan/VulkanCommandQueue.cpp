#include "VulkanCommandQueue.h"
#include "VulkanFence.h"
#include "VulkanCommandList.h"

namespace adria
{
	VulkanCommandQueue::VulkanCommandQueue(VkDevice device, VkQueue queue, GfxCommandListType type, Uint32 family_index)
		: device(device), queue(queue), type(type), family_index(family_index)
	{
	}

	void VulkanCommandQueue::ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists)
	{
		std::vector<VkCommandBufferSubmitInfo> cmd_infos;
		cmd_infos.reserve(cmd_lists.size());
		for (GfxCommandList* cmd : cmd_lists)
		{
			VulkanCommandList* vk_cmd = static_cast<VulkanCommandList*>(cmd);
			VkCommandBufferSubmitInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
			info.commandBuffer = vk_cmd->GetCommandBuffer();
			cmd_infos.push_back(info);
		}

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.commandBufferInfoCount = (Uint32)cmd_infos.size();
		submit.pCommandBufferInfos    = cmd_infos.data();

		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));
	}

	void VulkanCommandQueue::Signal(GfxFence& fence, Uint64 fence_value)
	{
		VulkanFence& vk_fence = static_cast<VulkanFence&>(fence);

		VkSemaphoreSubmitInfo signal_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signal_info.semaphore = vk_fence.GetSemaphore();
		signal_info.value     = fence_value;
		signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.signalSemaphoreInfoCount = 1;
		submit.pSignalSemaphoreInfos    = &signal_info;

		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));
	}

	void VulkanCommandQueue::Wait(GfxFence& fence, Uint64 fence_value)
	{
		VulkanFence& vk_fence = static_cast<VulkanFence&>(fence);

		VkSemaphoreSubmitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		wait_info.semaphore = vk_fence.GetSemaphore();
		wait_info.value     = fence_value;
		wait_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount = 1;
		submit.pWaitSemaphoreInfos    = &wait_info;

		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));
	}
}
