#pragma once
#include "Editor/ImGuiManager.h"

namespace adria
{

	class GfxDevice;
	class D3D12Device;
	template<Bool>
	class GfxRingDescriptorAllocator;
	class GfxCommandList;

	using GUIDescriptorAllocator = GfxRingDescriptorAllocator<false>;

	class D3D12ImGuiManager : public ImGuiManager
	{
	public:
		explicit D3D12ImGuiManager(GfxDevice* gfx);
		virtual ~D3D12ImGuiManager() override;

		virtual void Begin() const override;
		virtual void End(GfxCommandList* cmd_list) const override;

		virtual void ToggleVisibility() override;
		virtual Bool IsVisible() const override;

		virtual void ShowImage(GfxDescriptor image_descriptor, ImVec2 image_size) override;
		virtual void OnWindowEvent(WindowEventInfo const&) const override;

	private:
		D3D12Device* d3d12_gfx;
		std::string ini_file;
		std::unique_ptr<GUIDescriptorAllocator> imgui_allocator;
		Bool visible = true;
		mutable Uint64 frame_count = 0;

	private:
		GfxDescriptor AllocateDescriptorsGPU(Uint32 count = 1) const;
	};

}