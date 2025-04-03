#pragma once
#include "GPUDebugFeature.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	class GPUDebugPrinter : public GPUDebugFeature
	{
	public:
		explicit GPUDebugPrinter(GfxDevice* gfx);
		ADRIA_NONCOPYABLE(GPUDebugPrinter)
		ADRIA_DEFAULT_MOVABLE(GPUDebugPrinter)
		~GPUDebugPrinter();

		Int32 GetPrintfBufferIndex();
		void AddClearPass(RenderGraph& rg);
		void AddPrintPass(RenderGraph& rg);

	private:
		virtual void ProcessBufferData(GfxBuffer&) override;
	};
}