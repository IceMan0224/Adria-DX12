#include "D3D12DescriptorAllocator.h"

namespace adria
{
	D3D12DescriptorAllocator::D3D12DescriptorAllocator(std::unique_ptr<D3D12DescriptorHeap> heap)
		: heap(std::move(heap))
	{
		D3D12Descriptor head_descriptor = this->heap->GetDescriptor(0);
		D3D12Descriptor tail_descriptor = this->heap->GetDescriptor(this->heap->GetCapacity() - 1);
		free_descriptor_ranges.emplace_back(head_descriptor, tail_descriptor);
	}

	D3D12DescriptorAllocator::~D3D12DescriptorAllocator() = default;

	D3D12Descriptor D3D12DescriptorAllocator::AllocateDescriptor()
	{
		ADRIA_ASSERT(!free_descriptor_ranges.empty() && "Out of descriptor space!");
		D3D12DescriptorRange& range = free_descriptor_ranges.front();
		D3D12Descriptor handle = range.begin;

		range.begin.Increment(1);
		if (range.begin == range.end)
		{
			free_descriptor_ranges.pop_front();
		}
		return handle;
	}

	void D3D12DescriptorAllocator::FreeDescriptor(D3D12Descriptor handle)
	{
		D3D12Descriptor next_handle = handle;
		next_handle.Increment(1);

		D3D12DescriptorRange new_range{ .begin = handle, .end = next_handle };
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
			else if (it->begin.index > handle.index)
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

