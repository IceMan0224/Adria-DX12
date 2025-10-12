#pragma once
#include "PostEffect.h"
#include "Graphics/GfxPipelineState.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class CRTFilterPass : public PostEffect
	{
	public:
		CRTFilterPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual void OnSceneInitialized() {}
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> crt_pso;

	private:
		void CreatePSO();
	};

}