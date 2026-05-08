#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxCommandQueue.h"

namespace adria
{
	class VulkanCommandQueue final : public GfxCommandQueue
	{
	public:
		VulkanCommandQueue(VkDevice device, VkQueue queue, GfxCommandListType type, Uint32 family_index);
		virtual ~VulkanCommandQueue() override = default;

		virtual void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists) override;
		virtual void Signal(GfxFence& fence, Uint64 fence_value) override;
		virtual void Wait(GfxFence& fence, Uint64 fence_value) override;
		virtual Uint64 GetTimestampFrequency() const override { return timestamp_frequency; }
		virtual GfxCommandListType GetType() const override  { return type; }
		virtual void* GetNative() const override             { return (void*)queue; }

		VkQueue  GetQueue()       const { return queue; }
		Uint32   GetFamilyIndex() const { return family_index; }

		void SetTimestampFrequency(Uint64 freq) { timestamp_frequency = freq; }

	private:
		VkDevice           device             = VK_NULL_HANDLE;
		VkQueue            queue              = VK_NULL_HANDLE;
		GfxCommandListType type;
		Uint32             family_index       = 0;
		Uint64             timestamp_frequency = 1;
	};
}
