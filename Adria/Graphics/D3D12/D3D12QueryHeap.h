#pragma once
#include "Graphics/GfxQueryHeap.h"

namespace adria
{
	class D3D12Device;

	class D3D12QueryHeap final : public GfxQueryHeap
	{
		friend class D3D12Device;

	public:
		virtual void* GetHandle() const override { return query_heap.Get(); }

	private:
		Ref<ID3D12QueryHeap> query_heap;

	private:
		D3D12QueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);
	};
}