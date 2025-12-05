#pragma once
#include "Editor/ImGuiManager.h"

namespace adria
{
	class GfxDevice;
	class MetalDevice;
	class GfxCommandList;

	class MetalImGuiManager : public ImGuiManager
	{
	public:
		explicit MetalImGuiManager(GfxDevice* gfx);
		virtual ~MetalImGuiManager() override;

		virtual void Begin() const override;
		virtual void End(GfxCommandList* cmd_list) const override;

		virtual void ToggleVisibility() override;
		virtual Bool IsVisible() const override;

		virtual void ShowImage(GfxDescriptor image_descriptor, ImVec2 image_size) override;
		virtual void OnWindowEvent(WindowEventInfo const&) const override;

	private:
		MetalDevice* metal_gfx;
		std::string ini_file;
		Bool visible = true;
	};
}
