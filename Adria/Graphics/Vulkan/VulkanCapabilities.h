#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxCapabilities.h"

namespace adria
{
	class VulkanCapabilities final : public GfxCapabilities
	{
	public:
		virtual Bool Initialize(GfxDevice* gfx) override;
	};
}
