#include "GfxPIX.h"

#if defined(GFX_PIX_AVAILABLE)
#include "Core/Paths.h"
#include "Core/ConsoleManager.h"
#include "Utilities/StringUtil.h"
#include "pix3.h"
#endif
#include "Core/Logging/Log.h"

namespace adria::GfxPIX
{
#if defined(GFX_PIX_AVAILABLE)

	static AutoConsoleCommand PIX_TakeCapture("r.PIX", " Takes PIX capture. Optional arguments are: [capture name, frame count]",
		ConsoleCommandWithArgsDelegate::CreateLambda([](std::span<Char const*> args)
			{
				if (args.empty())
				{
					std::string capture_full_path = paths::PixCapturesDir + "Adria";
					GFX_PIX_TAKE_CAPTURE(capture_full_path.c_str(), 1);
				}
				else if (args.size() == 1)
				{
					Char const* arg = args[0];
					std::string capture_full_path = paths::PixCapturesDir + arg;
					GFX_PIX_TAKE_CAPTURE(capture_full_path.c_str(), 1);
				}
				else
				{
					Char const* arg = args[0];
					Uint64 pos;
					Uint32 frame_count = std::stoul(args[1], &pos);
					if (pos == strlen(args[1]))
					{
						std::string capture_full_path = paths::PixCapturesDir + arg;
						GFX_PIX_TAKE_CAPTURE(capture_full_path.c_str(), frame_count);
					}
				}
			}));

	namespace
	{
		Bool g_PixLoaded = false;
	}

	void Init()
	{
		HMODULE pix_library = PIXLoadLatestWinPixGpuCapturerLibrary();
		if (pix_library)
		{
			g_PixLoaded = true;
			ADRIA_LOG(INFO, "[PIX] PIX dll loaded!");
		}
		else
		{
			g_PixLoaded = false;
			ADRIA_LOG(WARNING, "[PIX] PIX dll could not be loaded!");
		}
	}

	void TakeCapture(Char const* capture_name, Uint32 num_frames)
	{
		ADRIA_ASSERT(num_frames != 0);
		if (!g_PixLoaded)
		{
			ADRIA_LOG(WARNING, "All PIX capture requests will be ignored because PIX dll wasn't loaded! Did you pass -pix as a command line argument?");
			return;
		}
		static Uint32 capture_index = 0;

		std::string full_capture_name = std::string(capture_name) + "_" + std::to_string(capture_index++) + ".wpix";
		std::wstring wcapture_name = ToWideString(full_capture_name);
		GFX_CHECK_HR(PIXGpuCaptureNextFrames(wcapture_name.c_str(), num_frames));
		ADRIA_LOG(INFO, "[PIX] Saving capture of %d frame(s) to %s...", num_frames, full_capture_name.c_str());
	}
#else

	void EmitWarning()
	{
		ADRIA_LOG(WARNING, "[PIX] PIX is not available in Release builds");
	}

#endif
}


