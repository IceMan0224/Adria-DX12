#include "VulkanTimestampProfiler.h"
#include "VulkanDevice.h"
#include "VulkanCommandList.h"
#include "VulkanQueryHeap.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxQueryHeap.h"

namespace adria
{
	VulkanTimestampProfiler::VulkanTimestampProfiler()
		: profiler_trees{ GfxProfilerTree(profile_allocators[0]),
		                  GfxProfilerTree(profile_allocators[1]),
		                  GfxProfilerTree(profile_allocators[2]) }
	{
	}

	void VulkanTimestampProfiler::Initialize(GfxDevice* _gfx)
	{
#if GFX_PROFILING
		if (_gfx->GetBackend() != GfxBackend::Vulkan)
		{
			ADRIA_ASSERT_MSG(false, "VulkanTimestampProfiler requires the Vulkan backend");
			return;
		}
		gfx = _gfx;

		query_readback_buffer = gfx->CreateBuffer(
			ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(Uint64)));

		GfxQueryHeapDesc query_heap_desc{};
		query_heap_desc.count = (Uint32)(MAX_PROFILES * 2 * FRAME_COUNT);
		query_heap_desc.type  = GfxQueryType::Timestamp;
		query_heap = gfx->CreateQueryHeap(query_heap_desc);
#endif
	}

	void VulkanTimestampProfiler::Shutdown()
	{
#if GFX_PROFILING
		query_heap.reset();
		query_readback_buffer.reset();
		gfx = nullptr;
#endif
	}

	void VulkanTimestampProfiler::NewFrame()
	{
#if GFX_PROFILING
		ADRIA_ASSERT(query_data.empty());
		current_frame = (current_frame + 1) % FRAME_COUNT;
		current_profiler_tree = &profiler_trees[current_frame];
		current_profiler_tree->Clear();
		profile_allocators[current_frame].Reset();
		scope_counter = 0;
		frame_reset_pending = true;
#endif
	}

	void VulkanTimestampProfiler::BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active)
	{
#if GFX_PROFILING
		if (!active)
		{
			return;
		}
		std::lock_guard lock(profiler_mutex);
		Uint32 profile_index = scope_counter++;
		ADRIA_ASSERT(profile_index < MAX_PROFILES);
		GfxProfilerTreeNode* tree_node = nullptr;
		if (!query_data.empty())
		{
			QueryData& parent_data = query_data.top();
			tree_node = parent_data.tree_node->EmplaceChild(name, cmd_list, profile_index, 0.0f);
		}
		else
		{
			ADRIA_ASSERT(current_profiler_tree->GetRoot() == nullptr);
			current_profiler_tree->EmplaceRoot(name, cmd_list, profile_index, 0.0f);
			tree_node = current_profiler_tree->GetRoot();
		}
		query_data.emplace(cmd_list, tree_node);

		Uint32 const begin_slot = (Uint32)(current_frame * MAX_PROFILES * 2) + profile_index * 2;
		if (frame_reset_pending)
		{
			VulkanCommandList* vk_cmd = static_cast<VulkanCommandList*>(cmd_list);
			VulkanQueryHeap*   vk_heap = static_cast<VulkanQueryHeap*>(query_heap.get());
			Uint32 const first = (Uint32)(current_frame * MAX_PROFILES * 2);
			vkCmdResetQueryPool(vk_cmd->GetCommandBuffer(), vk_heap->GetPool(), first, (Uint32)(MAX_PROFILES * 2));
			frame_reset_pending = false;
		}
		cmd_list->BeginQuery(*query_heap, begin_slot);
#endif
	}

	void VulkanTimestampProfiler::EndProfileScope(GfxCommandList* cmd_list)
	{
#if GFX_PROFILING
		ADRIA_ASSERT(!query_data.empty());
		std::lock_guard lock(profiler_mutex);
		QueryData& scope_data = query_data.top();
		ADRIA_ASSERT(scope_data.cmd_list == cmd_list);
		query_data.pop();
		Uint32 profile_index = scope_data.tree_node->GetData().index;

		Uint32 const end_slot = (Uint32)(current_frame * MAX_PROFILES * 2) + profile_index * 2 + 1;
		cmd_list->EndQuery(*query_heap, end_slot);
#endif
	}

	GfxProfilerTree const* VulkanTimestampProfiler::GetProfilerTree() const
	{
#if GFX_PROFILING
		Uint64 gpu_frequency = 0;
		gfx->GetTimestampFrequency(gpu_frequency);
		GfxCommandList* cmd_list = gfx->GetGraphicsCommandList();
		VulkanCommandList* vk_cmd = static_cast<VulkanCommandList*>(cmd_list);

		VkMemoryBarrier2 mem_barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		mem_barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		mem_barrier.srcAccessMask = VK_ACCESS_2_NONE;
		mem_barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
		mem_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.memoryBarrierCount = 1;
		dep.pMemoryBarriers    = &mem_barrier;
		vkCmdPipelineBarrier2(vk_cmd->GetCommandBuffer(), &dep);

		Uint32 const first_slot       = (Uint32)(current_frame * MAX_PROFILES * 2);
		Uint32 const written_count    = scope_counter * 2;
		Uint64 const slice_bytes_off  = first_slot * sizeof(Uint64);
		if (written_count > 0)
		{
			cmd_list->ResolveQueryData(*query_heap, first_slot, written_count,
				*query_readback_buffer, slice_bytes_off);
		}

		Uint64 const* query_timestamps = query_readback_buffer->GetMappedData<Uint64>();
		Uint64 const* frame_query_timestamps = query_timestamps + (current_frame * MAX_PROFILES * 2);

		current_profiler_tree->TraversePreOrder([this, gpu_frequency, frame_query_timestamps](GfxProfilerTreeNode* node)
		{
			Uint32 const index = node->GetData().index;
			Uint64 start_time = frame_query_timestamps[index * 2 + 0];
			Uint64 end_time   = frame_query_timestamps[index * 2 + 1];
			Uint64 delta      = (end_time >= start_time) ? (end_time - start_time) : 0;
			Float  frequency  = (Float)gpu_frequency;
			node->GetData().time = frequency > 0.0f ? (delta / frequency) * 1000.0f : 0.0f;
		});

		GfxProfilerTreeNode* root = current_profiler_tree->GetRoot();
		if (root && root->GetChildren().size() > 0)
		{
			Uint64 span_start = UINT64_MAX;
			Uint64 span_end   = 0;
			current_profiler_tree->TraversePreOrder([frame_query_timestamps, root, &span_start, &span_end](GfxProfilerTreeNode* node)
			{
				if (node == root) return;
				Uint32 const idx = node->GetData().index;
				Uint64 s = frame_query_timestamps[idx * 2 + 0];
				Uint64 e = frame_query_timestamps[idx * 2 + 1];
				if (s != 0 && s < span_start) span_start = s;
				if (e > span_end) span_end = e;
			});
			if (span_end > span_start && span_start != UINT64_MAX)
			{
				Float frequency = (Float)gpu_frequency;
				root->GetData().time = frequency > 0.0f ? ((span_end - span_start) / frequency) * 1000.0f : 0.0f;
			}
		}
		return current_profiler_tree;
#else
		return nullptr;
#endif
	}
}
