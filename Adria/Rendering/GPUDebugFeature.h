#pragma once
#include "Graphics/GfxMacros.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	class GpuDebugFeature
	{
	protected:
		explicit GpuDebugFeature(GfxDevice* gfx, RGResourceName gpu_buffer_name);
		ADRIA_NONCOPYABLE(GpuDebugFeature)
		ADRIA_DEFAULT_MOVABLE(GpuDebugFeature)
		virtual ~GpuDebugFeature();

		Int32 GetBufferIndex();
		void AddClearPass(RenderGraph& rg, Char const* pass_name);
		void AddFeaturePass(RenderGraph& rg, Char const* pass_name);

	protected:
		GfxDevice* gfx;
		std::unique_ptr<GfxBuffer> gpu_buffer;
		std::unique_ptr<GfxBuffer> cpu_readback_buffers[GFX_BACKBUFFER_COUNT];
		GfxDescriptor srv_descriptor;
		GfxDescriptor uav_descriptor;
		GfxDescriptor gpu_uav_descriptor;
		RGResourceName gpu_buffer_name;

	private:
		virtual void ProcessBufferData(GfxBuffer&) = 0;
	};
}