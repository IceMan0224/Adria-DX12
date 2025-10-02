#pragma once
#include "Graphics/GfxCommandQueue.h"

namespace adria
{
	class D3D12CommandQueue : public GfxCommandQueue
	{
	public:
		D3D12CommandQueue(GfxDevice* gfx, GfxCommandListType type, Char const* name = "");
		virtual ~D3D12CommandQueue() = default;

		virtual void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists) override;
		virtual void Signal(GfxFence& fence, Uint64 fence_value) override;
		virtual void Wait(GfxFence& fence, Uint64 fence_value) override;
		ADRIA_FORCEINLINE virtual Uint64 GetTimestampFrequency() const override { return timestamp_frequency; }
		ADRIA_FORCEINLINE virtual GfxCommandListType GetType() const override { return type; }
		ADRIA_FORCEINLINE virtual void* GetHandle() const override { return command_queue.Get(); }

	private:
		Ref<ID3D12CommandQueue> command_queue;
		Uint64 timestamp_frequency = 0;
		GfxCommandListType type;
	};
}