#pragma once

#if defined(ADRIA_PLATFORM_WINDOWS) && !defined(_RELEASE)
#define GFX_RENDERDOC_AVAILABLE
#endif

namespace adria
{
#if defined(GFX_RENDERDOC_AVAILABLE)

	namespace GfxRenderDoc
	{
		ADRIA_MAYBE_UNUSED Bool Init();
		Bool IsConnected();

		void SetCaptureFile(Char const* name);
		void StartCapture();
		void EndCapture();
		void EndFrame();
		void TriggerMultiFrameCapture(Uint32 frameCount);
	}

	#define GFX_RENDERDOC_INIT()				GfxRenderDoc::Init()
	#define GFX_RENDERDOC_IS_CONNECTED()        GfxRenderDoc::IsConnected()
	#define GFX_RENDERDOC_SETCAPFILE(filename)  GfxRenderDoc::SetCaptureFile(filename)
	#define GFX_RENDERDOC_STARTCAPTURE()        GfxRenderDoc::StartCapture()
	#define GFX_RENDERDOC_ENDCAPTURE()          GfxRenderDoc::EndCapture()
	#define GFX_RENDERDOC_MULTIFRAMECAPTURE(n)  GfxRenderDoc::TriggerMultiFrameCapture(n)
	#define GFX_RENDERDOC_ENDFRAME()            GfxRenderDoc::EndFrame()

#else 

	namespace GfxRenderDoc
	{
		void EmitWarning();
	}

	#define GFX_RENDERDOC_INIT()				GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_IS_CONNECTED()        GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_SETCAPFILE(filename)  GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_STARTCAPTURE()        GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_ENDCAPTURE()          GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_MULTIFRAMECAPTURE(n)  GfxRenderDoc::EmitWarning()
	#define GFX_RENDERDOC_ENDFRAME()            do {} while(0)

#endif
}