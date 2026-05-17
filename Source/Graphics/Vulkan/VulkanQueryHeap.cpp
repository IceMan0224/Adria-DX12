#include "VulkanQueryHeap.h"
#include "VulkanDevice.h"
#include "VulkanConversions.h"

namespace adria
{
	VulkanQueryHeap::VulkanQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& in_desc)
		: GfxQueryHeap(gfx, in_desc)
	{
		device = static_cast<VulkanDevice*>(gfx)->GetVkDevice();

		VkQueryPoolCreateInfo ci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		ci.queryType  = ConvertQueryType(in_desc.type);
		ci.queryCount = in_desc.count;
		if (in_desc.type == GfxQueryType::PipelineStatistics)
		{
			ci.pipelineStatistics =
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
				VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
				VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
				VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
		}

		VK_CHECK(vkCreateQueryPool(device, &ci, nullptr, &query_pool));
	}

	VulkanQueryHeap::~VulkanQueryHeap()
	{
		if (query_pool != VK_NULL_HANDLE)
		{
			vkDestroyQueryPool(device, query_pool, nullptr);
		}
	}
}
