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
		std::vector<VkSemaphoreSubmitInfo>     wait_infos;
		std::vector<VkSemaphoreSubmitInfo>     signal_infos;
		cmd_infos.reserve(cmd_lists.size());
		for (GfxCommandList* cmd : cmd_lists)
		{
			VulkanCommandList* vk_cmd = static_cast<VulkanCommandList*>(cmd);
			VkCommandBufferSubmitInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
			info.commandBuffer = vk_cmd->GetCommandBuffer();
			cmd_infos.push_back(info);
			for (auto const& [sem, val] : vk_cmd->GetPendingWaits())
			{
				VkSemaphoreSubmitInfo wi{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
				wi.semaphore = sem;
				wi.value     = val;
				wi.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
				wait_infos.push_back(wi);
			}
			for (auto const& [sem, val] : vk_cmd->GetPendingSignals())
			{
				VkSemaphoreSubmitInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
				si.semaphore = sem;
				si.value     = val;
				si.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
				signal_infos.push_back(si);
			}
			vk_cmd->GetPendingWaits().clear();
			vk_cmd->GetPendingSignals().clear();
		}

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount   = (Uint32)wait_infos.size();
		submit.pWaitSemaphoreInfos      = wait_infos.data();
		submit.commandBufferInfoCount   = (Uint32)cmd_infos.size();
		submit.pCommandBufferInfos      = cmd_infos.data();
		submit.signalSemaphoreInfoCount = (Uint32)signal_infos.size();
		submit.pSignalSemaphoreInfos    = signal_infos.data();

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
