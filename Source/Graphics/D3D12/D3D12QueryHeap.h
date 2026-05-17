#pragma once
#include "Graphics/GfxQueryHeap.h"

namespace adria
{
	class D3D12QueryHeap final : public GfxQueryHeap
	{
	public:
		D3D12QueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);

		virtual void* GetHandle() const override { return query_heap.Get(); }

	private:
		Ref<ID3D12QueryHeap> query_heap;
	};
}