#pragma once
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxTexture;
	struct WindowEventInfo;

	class ImGuiManager 
	{
	public:
		virtual ~ImGuiManager() = default;

		virtual void Begin() const = 0;
		virtual void End(GfxCommandList* cmd_list) const = 0;

		virtual void ToggleVisibility() = 0;
		virtual Bool IsVisible() const = 0;

		virtual void OnWindowEvent(WindowEventInfo const&) const = 0;

		virtual void ShowImage(GfxTexture const& final_texture, ImVec2 image_size = ImVec2(48.0f, 48.0f)) = 0;
	};

	std::unique_ptr<ImGuiManager> CreateImguiManager(GfxDevice* gfx);
}