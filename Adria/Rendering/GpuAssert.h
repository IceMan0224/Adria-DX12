#pragma once
#include "GpuDebugFeature.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	class GpuAssert : public GpuDebugFeature
	{
	public:
		explicit GpuAssert(GfxDevice* gfx);
		ADRIA_NONCOPYABLE(GpuAssert)
		ADRIA_DEFAULT_MOVABLE(GpuAssert)
		~GpuAssert();

		Int32 GetAssertBufferIndex();
		void AddClearPass(RenderGraph& rg);
		void AddAssertPass(RenderGraph& rg);

	private:
		virtual void ProcessBufferData(GfxBuffer&) override;
	};
}