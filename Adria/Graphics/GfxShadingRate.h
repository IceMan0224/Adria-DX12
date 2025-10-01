#pragma once
#include "Utilities/Enum.h"

namespace adria
{
	static constexpr Uint32 SHADING_RATE_SHIFT = 3;
	static constexpr Uint32 MAX_SHADING_RATES  = 7;
	static constexpr Uint32 SHADING_RATE_COMBINER_COUNT = 2;
	

	enum GfxShadingRate1D : Uint32
	{
		GfxShadingRate1D_1X = BIT(0),
		GfxShadingRate1D_2X = BIT(1),
		GfxShadingRate1D_4X = BIT(2)
	};

	enum class GfxVariableShadingMode : Uint32
	{
		None = 0,
		PerDraw,
		Image
	};

	enum GfxShadingRate : Uint32
	{
		GfxShadingRate_1X1 = (GfxShadingRate1D_1X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_1X2 = (GfxShadingRate1D_1X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_2X1 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_2X2 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_2X4 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X,
		GfxShadingRate_4X2 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_4X4 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X
	};

	enum class GfxShadingRateCombiner : Uint32
	{
		Passthrough,
		Override,   
		Min,
		Max,
		Sum,
	};

	class GfxTexture;
	struct GfxShadingRateInfo
	{
		GfxVariableShadingMode shading_mode = GfxVariableShadingMode::None;
		GfxShadingRate shading_rate = GfxShadingRate_1X1;
		GfxShadingRateCombiner shading_rate_combiner = GfxShadingRateCombiner::Passthrough;
		GfxTexture* shading_rate_image = nullptr;
	};
}