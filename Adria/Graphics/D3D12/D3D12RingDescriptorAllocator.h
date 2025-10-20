#pragma once
#include "D3D12DescriptorHeap.h"
#include "Utilities/RingOffsetAllocator.h" 

namespace adria
{
	template<Bool UseMutex>
	class D3D12RingDescriptorAllocator
	{
		using Mutex = std::conditional_t<UseMutex, std::mutex, DummyMutex>;

	public:
		D3D12RingDescriptorAllocator(std::unique_ptr<D3D12DescriptorHeap> heap, Uint32 reserve = 0)
			: heap(std::move(heap)), ring_offset_allocator(this->heap->GetCapacity(), reserve)
		{
		}
		ADRIA_NONCOPYABLE_NONMOVABLE(D3D12RingDescriptorAllocator)
		~D3D12RingDescriptorAllocator() = default;

		ADRIA_NODISCARD D3D12Descriptor Allocate(uint32_t count = 1)
		{
			Uint64 start_offset = INVALID_ALLOC_OFFSET;
			{
				std::lock_guard guard(alloc_mutex);
				start_offset = ring_offset_allocator.Allocate(count);
			}

			ADRIA_ASSERT_MSG(start_offset != INVALID_ALLOC_OFFSET, "Ring Descriptor Allocator has no free space!");
			if (start_offset == INVALID_ALLOC_OFFSET)
			{
				return D3D12Descriptor{};
			}
			return heap->GetDescriptor(static_cast<Uint32>(start_offset));
		}

		void FinishCurrentFrame(Uint64 frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_offset_allocator.FinishCurrentFrame(frame);
		}

		void ReleaseCompletedFrames(Uint64 completed_frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_offset_allocator.ReleaseCompletedFrames(completed_frame);
		}

		D3D12DescriptorHeap* GetHeap() const { return heap.get(); }

		D3D12Descriptor GetDescriptor(Uint32 index = 0) const
		{
			return heap->GetDescriptor(index);
		}

	private:
		std::unique_ptr<D3D12DescriptorHeap> heap;
		RingOffsetAllocator ring_offset_allocator;
		mutable Mutex alloc_mutex;
	};
}