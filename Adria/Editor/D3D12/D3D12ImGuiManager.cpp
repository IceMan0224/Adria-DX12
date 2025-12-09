#include "D3D12ImGuiManager.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "implot.h"
#include "Graphics/GfxDescriptor.h"
#include "IconsFontAwesome6.h"
#include "Core/Paths.h"
#include "Platform/Window.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/D3D12/D3D12RingDescriptorAllocator.h"
#include "Graphics/D3D12/D3D12Device.h"
#include "Graphics/D3D12/D3D12Conversions.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace adria
{
	D3D12ImGuiManager::D3D12ImGuiManager(GfxDevice* gfx) : d3d12_gfx((D3D12Device*)gfx)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		ini_file = paths::IniDir + "imgui.ini";
		io.IniFilename = ini_file.c_str();

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigWindowsResizeFromEdges = true;
		io.ConfigViewportsNoTaskBarIcon = true;

		ImFontConfig font_config{};
		std::string font_path = paths::FontsDir + "ComicMono/ComicMono.ttf";
		io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &font_config);
		font_config.MergeMode = true;
		ImWchar const icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		std::string icon_path = paths::FontsDir + "FontAwesome/" FONT_ICON_FILE_NAME_FAS;
		io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 15.0f, &font_config, icon_ranges);
		io.Fonts->Build();
		ImGui_ImplWin32_Init(gfx->GetWindowHandle());

		D3D12DescriptorHeapDesc gui_heap_desc{};
		gui_heap_desc.descriptor_count = 30;
		gui_heap_desc.shader_visible = true;
		gui_heap_desc.type = GfxDescriptorType::CBV_SRV_UAV;
		std::unique_ptr<D3D12DescriptorHeap> gui_heap = d3d12_gfx->CreateDescriptorHeap(gui_heap_desc);

		imgui_allocator = std::make_unique<GUIDescriptorAllocator>(std::move(gui_heap), 1);
		D3D12Descriptor handle = imgui_allocator->GetDescriptor(0);
		ImGui_ImplDX12_Init(d3d12_gfx->GetD3D12Device(), gfx->GetBackbufferCount(), DXGI_FORMAT_R8G8B8A8_UNORM,
							imgui_allocator->GetHeap()->GetD3D12Heap(), ToD3D12CPUHandle(handle), ToD3D12GPUHandle(handle));
	}

	D3D12ImGuiManager::~D3D12ImGuiManager()
	{
		d3d12_gfx->WaitForGPU();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void D3D12ImGuiManager::Begin() const
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		imgui_allocator->ReleaseCompletedFrames(frame_count);
	}

	void D3D12ImGuiManager::End(GfxCommandList* cmd_list) const
	{
		ImGui::Render();
		if (visible)
		{
			ID3D12GraphicsCommandList* d3d12_cmd_list = (ID3D12GraphicsCommandList*)cmd_list->GetNative();
			ID3D12DescriptorHeap* imgui_heap[] = { (ID3D12DescriptorHeap*)imgui_allocator->GetHeap()->GetD3D12Heap() };
			d3d12_cmd_list->SetDescriptorHeaps(ARRAYSIZE(imgui_heap), imgui_heap);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12_cmd_list);
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		imgui_allocator->FinishCurrentFrame(frame_count);
		++frame_count;
	}

	void D3D12ImGuiManager::ToggleVisibility()
	{
		visible = !visible;
	}
	Bool D3D12ImGuiManager::IsVisible() const
	{
		return visible;
	}

	void D3D12ImGuiManager::ShowImage(GfxTexture const& final_texture, ImVec2 image_size)
	{
		D3D12Descriptor dst_descriptor = imgui_allocator->Allocate(); 
		d3d12_gfx->GetD3D12Device()->CreateShaderResourceView((ID3D12Resource*)final_texture.GetNative(), nullptr, ToD3D12CPUHandle(dst_descriptor));
		ImGui::Image((ImTextureID)ToD3D12GPUHandle(dst_descriptor).ptr, image_size);
	}

	void D3D12ImGuiManager::OnWindowEvent(WindowEventInfo const& msg_data) const
	{
		ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(msg_data.handle),
			msg_data.msg, msg_data.wparam, msg_data.lparam);
	}
}
