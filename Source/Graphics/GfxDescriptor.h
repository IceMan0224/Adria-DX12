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
		Uint64 opaque_data[2] = { INVALID_OPAQUE_DATA, INVALID_OPAQUE_DATA };

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
		Usize operator()(GfxDescriptor const& d) const
		{
			return std::hash<Usize>{}(d.opaque_data[0]) + std::hash<Usize>{}(d.opaque_data[1]);
		}
	};
}