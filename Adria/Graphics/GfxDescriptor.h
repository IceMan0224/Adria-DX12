#pragma once

namespace adria
{
	enum class GfxDescriptorType : Uint8
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
		static constexpr Uint64 INVALID_OPAQUE_DATA = UINT64_MAX;

		Uint64 opaque_data[2] = { 0 };

		Bool operator==(GfxDescriptor const& other) const
		{
			return opaque_data[0] == other.opaque_data[0] && opaque_data[1] == other.opaque_data[1];
		}
		Bool IsValid() const
		{
			return opaque_data[0] != INVALID_OPAQUE_DATA || opaque_data[1] != INVALID_OPAQUE_DATA;
		}
	};

	struct GfxDescriptorHash
	{
		Uint64 operator()(GfxDescriptor const& d) const
		{
			return std::hash<Uint64>{}(d.opaque_data[0]) + std::hash<Uint64>{}(d.opaque_data[1]);
		}
	};


	struct GfxBindlessTable
	{
		Uint32 base = UINT32_MAX;
		Uint32 count = 0;
		GfxDescriptorType type = GfxDescriptorType::Invalid;

		Bool IsValid() const { return base != ~0u; }
		operator Uint32() const { return base; }
	};


	//struct GfxDescriptor
	//{
	//	GfxDescriptorHeap* parent_heap = nullptr;
	//	Uint32 index = static_cast<Uint32>(-1);
	//
	//	Uint32 GetIndex() const { return index; }
	//	void Increment(uint32_t multiply = 1)
	//	{
	//		index += multiply;
	//	}
	//	Bool operator==(GfxDescriptor const& other) const
	//	{
	//		return parent_heap == other.parent_heap && index == other.index;
	//	}
	//	Bool IsValid() const
	//	{
	//		return parent_heap != nullptr;
	//	}
	//};
}