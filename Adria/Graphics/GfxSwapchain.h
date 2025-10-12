#pragma once
#include "GfxFormat.h"
#include "GfxDefines.h"
#include "GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxTexture;

	struct GfxSwapchainDesc
	{
		Uint32 width = 0;
		Uint32 height = 0;
		GfxFormat backbuffer_format = GfxFormat::R8G8B8A8_UNORM_SRGB;
		Bool fullscreen_windowed = false;
	};

	class GfxSwapchain
	{
	public:
		virtual ~GfxSwapchain() {}

		virtual void SetAsRenderTarget(GfxCommandList* cmd_list)= 0;
		virtual void ClearBackbuffer(GfxCommandList* cmd_list)= 0;
		virtual Bool Present(Bool vsync) = 0;
		virtual void OnResize(Uint32 w, Uint32 h) = 0;

		virtual Uint32 GetBackbufferIndex() const = 0;
		virtual GfxTexture* GetBackbuffer() const = 0;
	};
}