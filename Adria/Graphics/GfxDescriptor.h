#pragma once

namespace adria
{
	class GfxDescriptorHeap; 
	enum class GfxDescriptorHeapType : Uint8
	{
		CBV_SRV_UAV,
		Sampler,
		RTV,
		DSV,
		Count,
		Invalid
	};

	struct GfxDescriptor
	{
		GfxDescriptorHeap* parent_heap = nullptr;
		Uint32 index = static_cast<Uint32>(-1);

		ADRIA_DEFAULT_COPYABLE_MOVABLE(GfxDescriptor)

		Uint32 GetIndex() const { return index; }
		void Increment(uint32_t multiply = 1)
		{
			index += multiply;
		}
		Bool operator==(GfxDescriptor const& other) const
		{
			return parent_heap == other.parent_heap && index == other.index;
		}
		Bool IsValid() const
		{
			return parent_heap != nullptr;
		}
	};
}