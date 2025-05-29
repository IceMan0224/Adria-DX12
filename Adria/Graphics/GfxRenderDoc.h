#pragma once


namespace adria
{
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
}