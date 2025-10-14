#pragma once
#include "GfxFence.h"
#include "GfxCommandListPool.h"

namespace adria
{
	class GfxFence;
	class GfxCommandList;
	class GfxCommandListPool;

	enum class GfxCommandListType : Uint8;

	class GfxCommandQueue
	{
	public:
		virtual ~GfxCommandQueue() = default;

		virtual void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists) = 0;
		virtual void Signal(GfxFence& fence, Uint64 fence_value) = 0;
		virtual void Wait(GfxFence& fence, Uint64 fence_value) = 0;

		virtual Uint64 GetTimestampFrequency() const = 0;
		virtual GfxCommandListType GetType() const = 0;
		virtual void* GetHandle() const = 0;
	};

	inline void ExecuteCommandListPool(GfxCommandQueue* queue, GfxCommandListPool& cmd_list_pool)
	{
		std::vector<GfxCommandList*> cmd_lists;
		for (auto& cmd_list : cmd_list_pool) { cmd_lists.push_back(cmd_list.get()); }
		queue->ExecuteCommandLists(cmd_lists);
	}
}