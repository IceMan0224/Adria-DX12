#include "GfxRenderDoc.h"
#include "Core/ConsoleManager.h"
#include "Core/Paths.h"
#include "Core/Logging/Log.h"
#include "renderdoc_app.h"

namespace adria::GfxRenderDoc
{
	static AutoConsoleCommand RenderDoc_TakeCapture("r.RenderDoc", " Takes RenderDoc capture. Optional arguments are: [capture name, frame count]",
		ConsoleCommandWithArgsDelegate::CreateLambda([](std::span<Char const*> args)
		{
			if (args.empty())
			{
				std::string capture_full_path = paths::RenderDocCapturesDir + "Adria";
				GfxRenderDoc::SetCaptureFile(capture_full_path.c_str());
				return GfxRenderDoc::TriggerMultiFrameCapture(1);
			}
			else if (args.size() == 1)
			{
				Char const* arg = args[0];
				std::string capture_full_path = paths::RenderDocCapturesDir + arg;
				GfxRenderDoc::SetCaptureFile(capture_full_path.c_str());
				return GfxRenderDoc::TriggerMultiFrameCapture(1);
			}
			else
			{
				Char const* arg = args[0];
				Uint64 pos;
				Uint32 frame_count = std::stoul(args[1], &pos);
				if (pos == strlen(args[1]))
				{
					std::string capture_full_path = paths::RenderDocCapturesDir + arg;
					GfxRenderDoc::SetCaptureFile(capture_full_path.c_str());
					return GfxRenderDoc::TriggerMultiFrameCapture(frame_count);
				}
			}
		}));

	namespace
	{
		constexpr Char const* g_RenderDocDLLName = "renderdoc.dll";

		RENDERDOC_API_1_6_0* g_RenderDocApi = nullptr;
		Bool	g_IsRenderDocCapturing = false;
		Bool	g_IsRenderDocLaunchedFromUi = false;
		Uint32  g_RenderDocNumCaptures = 0;
	}

	Bool Init()
	{
		if (g_RenderDocApi != nullptr)
		{
			return true;
		}

		HMODULE renderdoc_module = GetModuleHandleA(g_RenderDocDLLName);
		if (!renderdoc_module)
		{
			renderdoc_module = LoadLibraryA(g_RenderDocDLLName);
			if (!renderdoc_module)
			{
				ADRIA_LOG(WARNING, "[RenderDoc] Couldn't load the RenderDoc DLL.");
				return false;
			}
		}
		else
		{
			g_IsRenderDocLaunchedFromUi = true;
		}

		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(renderdoc_module, "RENDERDOC_GetAPI");
		if (RENDERDOC_GetAPI)
		{
			Int rval = (*RENDERDOC_GetAPI)(eRENDERDOC_API_Version_1_1_2, (void**)&g_RenderDocApi);
			if (rval != 1)
			{
				ADRIA_LOG(WARNING, "[RenderDoc] RENDERDOC_GetAPI failed with return code %d", rval);
				return false;
			}
			g_RenderDocApi->SetActiveWindow(nullptr, nullptr);

			RENDERDOC_InputButton captureKey = eRENDERDOC_Key_F12;
			g_RenderDocApi->SetCaptureKeys(&captureKey, 1);
			g_RenderDocApi->SetFocusToggleKeys(nullptr, 0);
			//g_RenderDocApi->MaskOverlayBits(eRENDERDOC_Overlay_None, eRENDERDOC_Overlay_None);
		}
	}

	Bool IsConnected()
	{
		return GetModuleHandleA(g_RenderDocDLLName) != nullptr && g_RenderDocApi != nullptr;
	}

	void SetCaptureFile(Char const* name)
	{
		if (!g_RenderDocApi)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] RenderDoc is not initialized, did you forget to use -renderdoc?");
			return;
		}
		if (g_IsRenderDocCapturing)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] There's already a capture running.");
			return;
		}
		if (g_RenderDocApi->SetCaptureFilePathTemplate)
		{
			g_RenderDocApi->SetCaptureFilePathTemplate(name);
		}
	}

	void StartCapture()
	{
		if (!g_RenderDocApi)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] RenderDoc is not initialized, did you forget to use -renderdoc?");
			return;
		}
		if (g_IsRenderDocCapturing)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] There's already a capture running.");
			return;
		}

		Bool initialized = Init();
		if (initialized)
		{
			g_IsRenderDocCapturing = true;
			if (g_RenderDocApi->StartFrameCapture)
			{
				g_RenderDocApi->StartFrameCapture(nullptr, nullptr);
			}
		}
	}

	void EndCapture()
	{
		if (!g_RenderDocApi)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] RenderDoc is not initialized, did you forget to use -renderdoc?");
			return;
		}
		if (!g_IsRenderDocCapturing)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] There's no capture to end. Did you forget to call GfxRenderDoc::BeginFrame?");
			return;
		}

		if (g_IsRenderDocCapturing)
		{
			if (g_RenderDocApi->EndFrameCapture)
			{
				g_RenderDocApi->EndFrameCapture(nullptr, nullptr);
			}
			g_IsRenderDocCapturing = false;
		}
	}

	void TriggerMultiFrameCapture(Uint32 frameCount)
	{
		if (!g_RenderDocApi)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] RenderDoc is not initialized, did you forget to use -renderdoc?");
			return;
		}

		if (g_IsRenderDocCapturing)
		{
			ADRIA_LOG(WARNING, "[RenderDoc] There's already a capture running.");
			return;
		}

		if (g_RenderDocApi->TriggerMultiFrameCapture)
		{
			g_RenderDocApi->TriggerMultiFrameCapture(frameCount);
		}
	}
}
