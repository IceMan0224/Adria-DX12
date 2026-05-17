#pragma once
#include "D3D12DescriptorHeap.h"

namespace adria
{
	class D3D12DescriptorAllocator
	{
		struct D3D12DescriptorRange
		{
			D3D12Descriptor begin;
			D3D12Descriptor end;
		};

	public:
		explicit D3D12DescriptorAllocator(std::unique_ptr<D3D12DescriptorHeap> heap);
		~D3D12DescriptorAllocator();
		ADRIA_NONCOPYABLE_NONMOVABLE(D3D12DescriptorAllocator)

		ADRIA_NODISCARD D3D12Descriptor AllocateDescriptor();
		void FreeDescriptor(D3D12Descriptor handle);

		ADRIA_FORCEINLINE D3D12DescriptorHeap* GetHeap() const { return heap.get(); }
		ADRIA_FORCEINLINE D3D12Descriptor GetDescriptor(Uint32 index = 0) const { return heap->GetDescriptor(index); }

	private:
		std::unique_ptr<D3D12DescriptorHeap> heap;
		std::list<D3D12DescriptorRange> free_descriptor_ranges;
	};
}