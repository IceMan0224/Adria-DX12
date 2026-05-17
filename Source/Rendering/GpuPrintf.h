#pragma once
#include "GpuDebugFeature.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	class GpuPrintf : public GpuDebugFeature
	{
	public:
		explicit GpuPrintf(GfxDevice* gfx);
		ADRIA_NONCOPYABLE(GpuPrintf)
		ADRIA_DEFAULT_MOVABLE(GpuPrintf)
		~GpuPrintf();

		Int32 GetPrintfBufferIndex();
		void AddClearPass(RenderGraph& rg);
		void AddPrintPass(RenderGraph& rg);

	private:
		virtual void ProcessBufferData(GfxBuffer&) override;
	};
}