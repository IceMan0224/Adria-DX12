#include "D3D12TimestampProfiler.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxQueryHeap.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxBufferView.h"

namespace adria
{
	D3D12TimestampProfiler::D3D12TimestampProfiler()
		: profiler_trees{ GfxProfilerTree(profile_allocators[0]), GfxProfilerTree(profile_allocators[1]), GfxProfilerTree(profile_allocators[2]) }
	{
	}

	void D3D12TimestampProfiler::Initialize(GfxDevice* _gfx)
	{
#if GFX_PROFILING
		if (_gfx->GetBackend() != GfxBackend::D3D12)
		{
			ADRIA_ASSERT_MSG(false, "Timestamp profiler only supports D3D12 for now!");
			return;
		}
		gfx = _gfx;
		query_readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(Uint64)));
		GfxQueryHeapDesc query_heap_desc{};
		query_heap_desc.count = MAX_PROFILES * 2;
		query_heap_desc.type = GfxQueryType::Timestamp;
		query_heap = gfx->CreateQueryHeap(query_heap_desc);
#endif
	}

	void D3D12TimestampProfiler::Shutdown()
	{
#if GFX_PROFILING
		query_heap.reset();
		query_readback_buffer.reset();
		gfx = nullptr;
#endif
	}

	void D3D12TimestampProfiler::NewFrame()
	{
#if GFX_PROFILING
		ADRIA_ASSERT(query_data.empty());
		current_profiler_tree = &profiler_trees[gfx->GetBackbufferIndex()];
		current_profiler_tree->Clear();
		profile_allocators[gfx->GetBackbufferIndex()].Reset();
		scope_counter = 0;
#endif
	}

	void D3D12TimestampProfiler::BeginProfileScope(GfxCommandList* cmd_list, const char* name, bool active)
	{
#if GFX_PROFILING
		if (!active)
		{
			return;
		}
#if GFX_MULTITHREADED
		std::lock_guard lock(profiler_mutex);
#endif
		Uint32 profile_index = scope_counter++;
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
		Uint32 begin_query_index = profile_index * 2;
		cmd_list->BeginQuery(*query_heap, begin_query_index);
#endif
	}

	void D3D12TimestampProfiler::EndProfileScope(GfxCommandList* cmd_list)
	{
#if GFX_PROFILING
		ADRIA_ASSERT(!query_data.empty());
#if GFX_MULTITHREADED
		std::lock_guard lock(profiler_mutex);
#endif
		QueryData& scope_data = query_data.top();
		ADRIA_ASSERT(scope_data.cmd_list == cmd_list);
		query_data.pop();
		Uint32 profile_index = scope_data.tree_node->GetData().index;
		Uint32 end_query_index = profile_index * 2 + 1;
		cmd_list->EndQuery(*query_heap, end_query_index);
#endif
	}

	GfxProfilerTree const* D3D12TimestampProfiler::GetProfilerTree() const
	{
#if GFX_PROFILING
		Uint64 gpu_frequency = 0;
		gfx->GetTimestampFrequency(gpu_frequency);
		Uint64 current_backbuffer_index = gfx->GetBackbufferIndex();

		current_profiler_tree->TraversePreOrder([this, current_backbuffer_index](GfxProfilerTreeNode* node)
			{
				Uint32 const index = node->GetData().index;
				GfxCommandList* cmd_list = node->GetData().cmd_list;
				ADRIA_ASSERT(index < MAX_PROFILES);
				ADRIA_ASSERT(cmd_list);
				Uint32 const begin_query_index = index * 2;
				Uint32 const end_query_index = index * 2 + 1;
				Uint64 readback_offset = ((current_backbuffer_index * MAX_PROFILES * 2) + begin_query_index) * sizeof(Uint64);
				cmd_list->ResolveQueryData(*query_heap, begin_query_index, 2, *query_readback_buffer, readback_offset);
			});

		Uint64 const* query_timestamps = query_readback_buffer->GetMappedData<Uint64>();
		Uint64 const* frame_query_timestamps = query_timestamps + (current_backbuffer_index * MAX_PROFILES * 2);

		current_profiler_tree->TraversePreOrder([this, gpu_frequency, frame_query_timestamps](GfxProfilerTreeNode* node)
			{
				Uint32 const index = node->GetData().index;
				Uint64 start_time = frame_query_timestamps[index * 2 + 0];
				Uint64 end_time = frame_query_timestamps[index * 2 + 1];
				Uint64 delta = end_time - start_time;
				Float frequency = Float(gpu_frequency);
				node->GetData().time = (delta / frequency) * 1000.0f;
			});
		return current_profiler_tree;
#else
		return nullptr;
#endif
	}
}