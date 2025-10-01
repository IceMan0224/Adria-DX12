#include "GfxDescriptorAllocator.h"

namespace adria
{
	GfxDescriptorAllocator::GfxDescriptorAllocator(std::unique_ptr<GfxDescriptorHeap> heap)
		: heap(std::move(heap))
	{
		GfxDescriptor head_descriptor = this->heap->GetDescriptor(0);
		GfxDescriptor tail_descriptor = this->heap->GetDescriptor(this->heap->GetCapacity());
		free_descriptor_ranges.emplace_back(head_descriptor, tail_descriptor);
	}

	GfxDescriptorAllocator::~GfxDescriptorAllocator() = default;

	GfxDescriptor GfxDescriptorAllocator::AllocateDescriptor()
	{
		ADRIA_ASSERT(!free_descriptor_ranges.empty() && "Out of descriptor space!");
		GfxDescriptorRange& range = free_descriptor_ranges.front();
		GfxDescriptor handle = range.begin;

		range.begin.Increment(1);
		if (range.begin == range.end)
		{
			free_descriptor_ranges.pop_front();
		}
		return handle;
	}

	void GfxDescriptorAllocator::FreeDescriptor(GfxDescriptor handle)
	{
		GfxDescriptor next_handle = handle;
		next_handle.Increment(1);

		GfxDescriptorRange new_range{ .begin = handle, .end = next_handle };
		Bool merged = false;
		for (auto it = free_descriptor_ranges.begin(); it != free_descriptor_ranges.end(); ++it)
		{
			if (it->end == handle)
			{
				it->end.Increment(1);
				auto next_it = std::next(it);
				if (next_it != free_descriptor_ranges.end() && next_it->begin == it->end)
				{
					it->end = next_it->end;
					free_descriptor_ranges.erase(next_it);
				}
				merged = true;
				break;
			}
			else if (it->begin == next_handle)
			{
				it->begin = handle;
				merged = true;
				break;
			}
			else if (it->begin.GetIndex() > handle.GetIndex())
			{
				free_descriptor_ranges.insert(it, new_range);
				merged = true;
				break;
			}
		}

		if (!merged)
		{
			free_descriptor_ranges.push_back(new_range);
		}
	}
}

