#pragma once
#include "GfxDescriptorHeap.h"

namespace adria
{
	class GfxDescriptorAllocator
	{
		struct GfxDescriptorRange
		{
			GfxDescriptor begin;
			GfxDescriptor end;
		};

	public:
		GfxDescriptorAllocator(std::unique_ptr<GfxDescriptorHeap> heap);
		~GfxDescriptorAllocator();
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxDescriptorAllocator)

		ADRIA_NODISCARD GfxDescriptor AllocateDescriptor();
		void FreeDescriptor(GfxDescriptor handle);

		GfxDescriptorHeap* GetHeap() const { return heap.get(); }

	private:
		std::unique_ptr<GfxDescriptorHeap> heap;
		std::list<GfxDescriptorRange> free_descriptor_ranges;
	};
}