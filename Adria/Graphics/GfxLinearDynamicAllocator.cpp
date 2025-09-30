#include "GfxLinearDynamicAllocator.h"
#include "GfxBuffer.h"
#include "GfxDevice.h"

namespace adria
{
	GfxLinearDynamicAllocator::GfxLinearDynamicAllocator(GfxDevice* gfx, Uint64 page_size, Uint64 page_count)
		: gfx(gfx), page_size(page_size), current_page(0), used_page_count_history{}
	{
		alloc_pages.reserve(page_count);
		while (alloc_pages.size() < page_count) alloc_pages.emplace_back(gfx, page_size);
	}
	GfxLinearDynamicAllocator::~GfxLinearDynamicAllocator() = default;

	GfxDynamicAllocation GfxLinearDynamicAllocator::Allocate(Uint64 size_in_bytes, Uint64 alignment)
	{
		alignment = std::max(alignment, 1ull);

		GfxAllocationPage* last_page = &alloc_pages[current_page]; // Use pointer to allow changing page
		Uint64 current_page_offset = last_page->linear_offset_allocator.Top(); 

		Uint64 base_gpu_address = last_page->buffer->GetGpuAddress();
		Uint64 current_gpu_address = base_gpu_address + current_page_offset;
		Uint64 aligned_gpu_address = AlignUp(current_gpu_address, alignment);
		Uint64 padding = aligned_gpu_address - current_gpu_address;
		Uint64 total_allocation_size = padding + size_in_bytes;

		Uint64 allocated_page_offset = INVALID_ALLOC_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			allocated_page_offset = last_page->linear_offset_allocator.Allocate(total_allocation_size, 1);
		}

		if (allocated_page_offset != INVALID_ALLOC_OFFSET)
		{
			Uint64 final_data_offset = allocated_page_offset + padding;

			GfxDynamicAllocation allocation{};
			allocation.buffer = last_page->buffer.get();
			allocation.cpu_address = reinterpret_cast<Uint8*>(last_page->cpu_address) + final_data_offset;
			allocation.gpu_address = base_gpu_address + final_data_offset;
			allocation.offset = final_data_offset; 
			allocation.size = size_in_bytes; 

			ADRIA_ASSERT_MSG(allocation.gpu_address % alignment == 0, "Dynamic allocation final GPU address is misaligned!");
			ADRIA_ASSERT_MSG(final_data_offset + size_in_bytes <= last_page->linear_offset_allocator.MaxSize(), "Dynamic allocation exceeds page bounds!");
			return allocation;
		}
		else 
		{
			++current_page;
			if (current_page >= alloc_pages.size())
			{
				Uint64 required_page_size = AlignUp(size_in_bytes, alignment);
				Uint64 new_page_size = std::max(page_size, std::max(size_in_bytes, required_page_size)); 
				alloc_pages.emplace_back(gfx, new_page_size);
			}
			return Allocate(size_in_bytes, alignment); 
		}
	}
	void GfxLinearDynamicAllocator::Clear()
	{
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			for (GfxAllocationPage& page : alloc_pages)
			{
				page.linear_offset_allocator.Clear();
			}
		}
		
		Uint32 i = gfx->GetFrameIndex() % PAGE_COUNT_HISTORY_SIZE;
		used_page_count_history[i] = current_page;

		Uint64 max_used_page_count = 0;
		for (Uint32 j = 0; j < PAGE_COUNT_HISTORY_SIZE; ++j)
		{
			max_used_page_count = std::max(max_used_page_count, used_page_count_history[j]);
		}

		if (max_used_page_count < alloc_pages.size())
		{
			while (alloc_pages.size() == max_used_page_count)
			{
				alloc_pages.pop_back();
			}
		}
		current_page = 0;
	}

	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxDevice* gfx, Uint64 page_size) : linear_offset_allocator(page_size)
	{
		GfxBufferDesc desc{};
		desc.size = page_size;
		desc.resource_usage = GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		buffer = gfx->CreateBuffer(desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
		buffer->SetName("LinearDynamicAllocatorPage");
	}
	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxAllocationPage&&) = default;

	GfxLinearDynamicAllocator::GfxAllocationPage::~GfxAllocationPage() = default;

}