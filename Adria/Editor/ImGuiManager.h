#pragma once
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	template<Bool>
	class GfxRingDescriptorAllocator;
	class GfxCommandList;

	using GUIDescriptorAllocator = GfxRingDescriptorAllocator<false>;

	struct WindowEventInfo;

	class ImGuiManager
	{
	public:
		explicit ImGuiManager(GfxDevice* gfx);
		~ImGuiManager();

		void Begin() const;
		void End(GfxCommandList* cmd_list) const;

		void ShowImage(GfxDescriptor image_descriptor, ImVec2 image_size = ImVec2(48.0f, 48.0f));

		void OnWindowEvent(WindowEventInfo const&) const;

		void ToggleVisibility();
		Bool IsVisible() const;

	private:
		GfxDevice* gfx;
		std::string ini_file;
		std::unique_ptr<GUIDescriptorAllocator> imgui_allocator;
		Bool visible = true;
		mutable Uint64 frame_count = 0;

	private:
		GfxDescriptor AllocateDescriptorsGPU(Uint32 count = 1) const;
	};

}