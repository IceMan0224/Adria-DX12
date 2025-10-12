#pragma once
#include "Graphics/GfxCapabilities.h"

namespace adria
{
	class D3D12Capabilities final : public GfxCapabilities
	{
	public:
		virtual Bool Initialize(GfxDevice* gfx) override;
	};
}

