#pragma once

#if defined(ADRIA_PLATFORM_WINDOWS) && !defined(_RELEASE)
#define GFX_PIX_AVAILABLE
#endif

namespace adria
{
#if defined(GFX_PIX_AVAILABLE)
	namespace D3D12PIX
	{
		void Init();
		void TakeCapture(Char const* capture_name, Uint32 num_frames);
	}

	#define GFX_PIX_INIT()	D3D12PIX::Init()
	#define GFX_PIX_TAKE_CAPTURE(name, frames)	D3D12PIX::TakeCapture(name, frames)

#else 
	namespace D3D12PIX
	{
		void EmitWarning();
	}

	#define GFX_PIX_INIT()	D3D12PIX::EmitWarning()
	#define GFX_PIX_TAKE_CAPTURE(name, frames)	D3D12PIX::EmitWarning()

#endif
}

