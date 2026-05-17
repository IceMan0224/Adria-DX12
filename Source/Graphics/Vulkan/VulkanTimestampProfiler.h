#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTimestampProfilerFwd.h"

namespace adria
{
	class GfxBuffer;
	class GfxQueryHeap;
	using GfxProfilerTreeNode = typename GfxProfilerTree::NodeType;

	class VulkanTimestampProfiler final : public GfxProfiler
	{
	public:
		VulkanTimestampProfiler();

		virtual void Initialize(GfxDevice* gfx) override;
		virtual void Shutdown() override;
		virtual void NewFrame() override;
		virtual void BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active = true) override;
		virtual void EndProfileScope(GfxCommandList* cmd_list) override;
		virtual GfxProfilerTree const* GetProfilerTree() const override;

	private:
		GfxDevice* gfx = nullptr;
		std::unique_ptr<GfxQueryHeap> query_heap;
		std::unique_ptr<GfxBuffer>    query_readback_buffer;

		static constexpr Uint64 FRAME_COUNT   = GFX_BACKBUFFER_COUNT;
		static constexpr Uint64 MAX_PROFILES  = 256;
		GfxProfilerTreeAllocator profile_allocators[FRAME_COUNT];
		GfxProfilerTree          profiler_trees[FRAME_COUNT];
		GfxProfilerTree*         current_profiler_tree = nullptr;

		Uint32 current_frame = 0;
		Bool   frame_reset_pending = false;

		struct QueryData
		{
			GfxCommandList*      cmd_list  = nullptr;
			GfxProfilerTreeNode* tree_node = nullptr;
		};
		std::stack<QueryData> query_data;
		Uint32 scope_counter = 0;
		ProfilerMutexT profiler_mutex;
	};
}
