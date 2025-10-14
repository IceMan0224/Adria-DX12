#include "ImGuiManager.h"
#include "D3D12/D3D12ImGuiManager.h"
#include "Graphics/GfxDevice.h"

namespace adria
{
	std::unique_ptr<ImGuiManager> CreateImguiManager(GfxDevice* gfx)
	{
		if (gfx->GetBackend() == GfxBackend::D3D12)
		{
			return std::make_unique<D3D12ImGuiManager>(gfx);
		}
		ADRIA_ASSERT_MSG(false, "No ImGuiManager implementation for this backend!");
		return nullptr;
	}
}

