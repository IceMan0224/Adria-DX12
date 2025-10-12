#include "GfxRenderDoc.h"
#if defined(GFX_RENDERDOC_AVAILABLE)
#include "Core/ConsoleManager.h"
#include "Core/Paths.h"
#include "renderdoc_app.h"
#endif


namespace adria::GfxRenderDoc
{
#if defined(GFX_RENDERDOC_AVAILABLE)

	ADRIA_LOG_CHANNEL(RenderDoc);

	static AutoConsoleCommand RenderDoc_TakeCapture("r.RenderDoc", " Takes RenderDoc capture. Optional arguments are: [capture name, frame count]",
		ConsoleCommandWithArgsDelegate::CreateLambda([](std::span<Char const*> args)
		{
			if (args.empty())
			{
				std::string capture_full_path = paths::RenderDocCapturesDir + "Adria";
				GFX_RENDERDOC_SETCAPFILE(capture_full_path.c_str());
				GFX_RENDERDOC_MULTIFRAMECAPTURE(1);
			}
			else if (args.size() == 1)
			{
				Char const* arg = args[0];
				std::string capture_full_path = paths::RenderDocCapturesDir + arg;
				GFX_RENDERDOC_SETCAPFILE(capture_full_path.c_str());
				GFX_RENDERDOC_MULTIFRAMECAPTURE(1);
			}
			else
			{
				Char const* arg = args[0];
				Uint64 pos;
				Uint32 frame_count = std::stoul(args[1], &pos);
				if (pos == strlen(args[1]))
				{
					std::string capture_full_path = paths::RenderDocCapturesDir + arg;
					GFX_RENDERDOC_SETCAPFILE(capture_full_path.c_str());
					GFX_RENDERDOC_MULTIFRAMECAPTURE(frame_count);
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
				ADRIA_LOG(WARNING, "Couldn't load the RenderDoc DLL.");
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
			Int rval = (*RENDERDOC_GetAPI)(eRENDERDOC_API_Version_1_6_0, (void**)&g_RenderDocApi);
			if (rval != 1)
			{
				ADRIA_LOG(WARNING, "RENDERDOC_GetAPI failed with return code %d", rval);
				return false;
			}
			g_RenderDocApi->SetActiveWindow(nullptr, nullptr);

			RENDERDOC_InputButton captureKey = eRENDERDOC_Key_F12;
			g_RenderDocApi->SetCaptureKeys(&captureKey, 1);
			g_RenderDocApi->SetFocusToggleKeys(nullptr, 0);
			g_RenderDocApi->MaskOverlayBits(eRENDERDOC_Overlay_None, eRENDERDOC_Overlay_None);
			return true;
		}
		return false;
	}

	Bool IsConnected()
	{
		return GetModuleHandleA(g_RenderDocDLLName) != nullptr && g_RenderDocApi != nullptr;
	}

	void SetCaptureFile(Char const* name)
	{
		if (!g_RenderDocApi)
		{
			ADRIA_LOG(WARNING, "RenderDoc is not initialized, did you forget to use -renderdoc?");
			return;
		}
		if (g_IsRenderDocCapturing)
		{
			ADRIA_LOG(WARNING, "There's already a capture running.");
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

	void EndFrame()
	{
		if (!IsConnected())
		{
			return;
		}

		if (g_RenderDocApi->GetNumCaptures && g_RenderDocApi->GetNumCaptures() > g_RenderDocNumCaptures)
		{
			g_RenderDocNumCaptures = g_RenderDocApi->GetNumCaptures();

			if (!g_RenderDocApi->IsTargetControlConnected())
			{
				Uint32 path_length = 0;
				g_RenderDocApi->GetCapture(g_RenderDocApi->GetNumCaptures() - 1, nullptr, &path_length, nullptr);
				if (path_length > 0)
				{
					Char* logFile = (Char*)alloca(path_length);
					g_RenderDocApi->GetCapture(g_RenderDocApi->GetNumCaptures() - 1, logFile, nullptr, nullptr);
					g_RenderDocApi->LaunchReplayUI(1, logFile);
				}
			}
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

#else

	void EmitWarning()
	{
		ADRIA_LOG(WARNING, "[RenderDoc] RenderDoc is not available in Release builds");
	}

#endif
}


