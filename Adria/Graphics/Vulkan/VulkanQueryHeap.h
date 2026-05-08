#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxQueryHeap.h"

namespace adria
{
	class VulkanQueryHeap final : public GfxQueryHeap
	{
	public:
		VulkanQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);
		virtual ~VulkanQueryHeap() override;

		virtual void* GetHandle() const override { return (void*)query_pool; }
		VkQueryPool   GetPool()   const          { return query_pool; }

	private:
		VkDevice    device     = VK_NULL_HANDLE;
		VkQueryPool query_pool = VK_NULL_HANDLE;
	};
}
