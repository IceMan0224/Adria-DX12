#pragma once
#include "PostEffect.h"
#include "Graphics/GfxPipelineStateFwd.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class StarrySkyPass : public PostEffect
	{
	public:
		StarrySkyPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> starry_sky_pso;
		Float stars_threshold = 8.0f;
		Float stars_exposure = 200.0f;

	private:
		void CreatePSO();
	};
}
