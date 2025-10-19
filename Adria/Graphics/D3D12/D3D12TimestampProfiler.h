#pragma once
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTimestampProfilerFwd.h"

namespace adria
{
	class GfxBuffer;
	class GfxQueryHeap;
	using GfxProfilerTreeNode = typename GfxProfilerTree::NodeType;

	class D3D12TimestampProfiler final : public GfxProfiler
	{
	public:
		D3D12TimestampProfiler();

		virtual void Initialize(GfxDevice* gfx) override;
		virtual void Shutdown() override;
		virtual void NewFrame() override;
		virtual void BeginProfileScope(GfxCommandList* cmd_list, const char* name, bool active = true) override;
		virtual void EndProfileScope(GfxCommandList* cmd_list) override;
		virtual GfxProfilerTree const* GetProfilerTree() const override;

	private:
		GfxDevice* gfx = nullptr;
		std::unique_ptr<GfxQueryHeap> query_heap;
		std::unique_ptr<GfxBuffer> query_readback_buffer;

		static constexpr Uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
		static constexpr Uint64 MAX_PROFILES = 256;
		GfxProfilerTreeAllocator profile_allocators[FRAME_COUNT];
		GfxProfilerTree profiler_trees[FRAME_COUNT];
		GfxProfilerTree* current_profiler_tree = nullptr;
		struct QueryData
		{
			GfxCommandList* cmd_list = nullptr;
			GfxProfilerTreeNode* tree_node = nullptr;
		};
		std::stack<QueryData> query_data;
		Uint32 scope_counter = 0;
#if GFX_MULTITHREADED
		std::mutex profiler_mutex;
#endif
	};
}