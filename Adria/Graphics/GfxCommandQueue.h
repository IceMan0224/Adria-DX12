#pragma once
#include "GfxFence.h"

namespace adria
{
	class GfxFence;
	class GfxCommandList;
	class GfxCommandListPool;

	enum class GfxCommandListType : Uint8;

	class IGfxCommandQueue
	{
	public:
		virtual ~IGfxCommandQueue() = default;

		virtual void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists) = 0;
		virtual void Signal(GfxFence& fence, Uint64 fence_value) = 0;
		virtual void Wait(GfxFence& fence, Uint64 fence_value) = 0;

		virtual Uint64 GetTimestampFrequency() const = 0;
		virtual GfxCommandListType GetType() const = 0;
		virtual void* GetHandle() const = 0;
	};
}