#pragma once
#include "GfxDescriptorHeap.h"
#include "Utilities/RingOffsetAllocator.h" 

namespace adria
{
	template<bool UseMutex>
	class GfxRingDescriptorAllocator
	{
		using Mutex = std::conditional_t<UseMutex, std::mutex, DummyMutex>;

	public:
		GfxRingDescriptorAllocator(std::unique_ptr<GfxDescriptorHeap> heap, Uint32 reserve = 0)
			: heap(std::move(heap)), ring_offset_allocator(this->heap->GetCapacity(), reserve)
		{
		}
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxRingDescriptorAllocator)
		~GfxRingDescriptorAllocator() = default;

		ADRIA_NODISCARD GfxDescriptor Allocate(uint32_t count = 1)
		{
			Uint64 start_offset = INVALID_ALLOC_OFFSET;
			{
				std::lock_guard guard(alloc_mutex);
				start_offset = ring_offset_allocator.Allocate(count);
			}

			ADRIA_ASSERT_MSG(start_offset != INVALID_ALLOC_OFFSET, "Ring Descriptor Allocator has no free space!");
			if (start_offset == INVALID_ALLOC_OFFSET)
			{
				return GfxDescriptor{}; 
			}
			return heap->GetDescriptor(static_cast<Uint32>(start_offset));
		}

		void FinishCurrentFrame(uint64_t frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_offset_allocator.FinishCurrentFrame(frame);
		}

		void ReleaseCompletedFrames(uint64_t completed_frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_offset_allocator.ReleaseCompletedFrames(completed_frame);
		}

		GfxDescriptorHeap* GetHeap() const { return heap.get(); }

		GfxDescriptor GetDescriptor(Uint32 index = 0) const
		{
			return heap->GetDescriptor(index);
		}

	private:
		std::unique_ptr<GfxDescriptorHeap> heap;
		mutable Mutex alloc_mutex;
		RingOffsetAllocator ring_offset_allocator;
	};
}