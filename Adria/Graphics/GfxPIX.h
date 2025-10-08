#pragma once

#if !defined(_RELEASE)
#define GFX_PIX_AVAILABLE
#endif

namespace adria
{
#if defined(GFX_PIX_AVAILABLE)
	namespace GfxPIX
	{
		void Init();
		void TakeCapture(Char const* capture_name, Uint32 num_frames);
	}

	#define GFX_PIX_INIT()	GfxPIX::Init()
	#define GFX_PIX_TAKE_CAPTURE(name, frames)	GfxPIX::TakeCapture(name, frames)

#else 
	namespace GfxPIX
	{
		void EmitWarning();
	}

	#define GFX_PIX_INIT()	GfxPIX::EmitWarning()
	#define GFX_PIX_TAKE_CAPTURE(name, frames)	GfxPIX::EmitWarning()

#endif
}

