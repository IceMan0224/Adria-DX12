#pragma once
#include "GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;

	enum class GfxCommonTextureType : Uint8
	{
		BlackTexture2D,
		WhiteTexture2D,
		DefaultNormal2D,
		MetallicRoughness2D,
		Count
	};

	enum class GfxCommonViewType : Uint8
	{
		BlackTexture2D_SRV,
		WhiteTexture2D_SRV,
		DefaultNormal2D_SRV,
		MetallicRoughness2D_SRV,
		Count
	};

	namespace GfxCommon
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();

		GfxTexture*   GetCommonTexture(GfxCommonTextureType);
		GfxDescriptor GetCommonView(GfxCommonViewType);
		Uint32		  GetCommonViewBindlessIndex(GfxCommonViewType);
	}
}