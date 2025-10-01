#pragma once
#include "Graphics/GfxFence.h"

namespace adria
{
	class D3D12Fence : public GfxFence
	{
	public:
		virtual ~D3D12Fence();

		virtual Bool Create(GfxDevice* gfx, Char const* name) override;
		virtual void Wait(Uint64 value) override;
		virtual void Signal(Uint64 value) override;
		virtual Bool IsCompleted(Uint64 value) override;
		virtual Uint64 GetCompletedValue() const override;
		virtual void* GetHandle() const override;

	private:
		Ref<ID3D12Fence> fence = nullptr;
		HANDLE event = nullptr;
	};
}