#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalImGuiManager.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_osx.h"
#include "imgui_impl_metal.h"
#include "implot.h"
#include "Core/Paths.h"
#include "Platform/Window.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/Metal/MetalDevice.h"
#include "Graphics/Metal/MetalCommandList.h"
#include "IconsFontAwesome6.h"
#include "Logging/Log.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(Editor);

	MetalImGuiManager::MetalImGuiManager(GfxDevice* gfx) : metal_gfx((MetalDevice*)gfx)
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
		io.ConfigWindowsResizeFromEdges = true;
		io.ConfigViewportsNoTaskBarIcon = true;

		ImFontConfig font_config{};
		std::string font_path = paths::FontsDir + "ComicMono/ComicMono.ttf";
		io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &font_config);
		font_config.MergeMode = true;
		ImWchar const icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		std::string icon_path = paths::FontsDir + "FontAwesome/" FONT_ICON_FILE_NAME_FAS;
		io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 15.0f, &font_config, icon_ranges);

		NSWindow* ns_window = (__bridge NSWindow*)gfx->GetWindowHandle();
		NSView* content_view = [ns_window contentView];

		ImGui_ImplOSX_Init(content_view);
		ImGui_ImplMetal_Init(metal_gfx->GetMTLDevice());
	}

	MetalImGuiManager::~MetalImGuiManager()
	{
		metal_gfx->WaitForGPU();
		ImGui_ImplMetal_Shutdown();
		ImGui_ImplOSX_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}

	void MetalImGuiManager::Begin() const
	{
		NSWindow* ns_window = (__bridge NSWindow*)metal_gfx->GetWindowHandle();
		NSView* content_view = [ns_window contentView];

		GfxTexture* backbuffer = metal_gfx->GetBackbuffer();

		MTLRenderPassDescriptor* render_pass_desc = [MTLRenderPassDescriptor new];
		if (backbuffer)
		{
			id<MTLTexture> metal_texture = (__bridge id<MTLTexture>)backbuffer->GetNative();
			render_pass_desc.colorAttachments[0].texture = metal_texture;
		}

		ImGui_ImplOSX_NewFrame(content_view);
		ImGui_ImplMetal_NewFrame(render_pass_desc);

		ImGui::NewFrame();
	}

	void MetalImGuiManager::End(GfxCommandList* cmd_list) const
	{
		ImGui::Render();

		if (visible)
		{
			MetalCommandList* metal_cmd_list = static_cast<MetalCommandList*>(cmd_list);
			id<MTLCommandBuffer> command_buffer = metal_cmd_list->GetCommandBuffer();
			id<MTLRenderCommandEncoder> render_encoder = metal_cmd_list->GetRenderEncoder();
			ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), command_buffer, render_encoder);
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void MetalImGuiManager::ToggleVisibility()
	{
		visible = !visible;
	}

	Bool MetalImGuiManager::IsVisible() const
	{
		return visible;
	}

	void MetalImGuiManager::ShowImage(GfxDescriptor image_descriptor, ImVec2 image_size)
	{
		// ImGui::Image((ImTextureID)(__bridge void*)texture, image_size);
	}

	void MetalImGuiManager::OnWindowEvent(WindowEventInfo const& msg_data) const
	{
		// ImGui_ImplOSX_NewFrame automatically handles display size from the view
		// No manual intervention needed
	}
}
