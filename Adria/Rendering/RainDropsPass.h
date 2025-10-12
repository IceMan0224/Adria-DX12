#pragma once
#include "PostEffect.h"
#include "TextureHandle.h"
#include "Graphics/GfxPipelineStateFwd.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;
	struct Light;

	class RainDropsPass : public PostEffect
	{
	public:
		RainDropsPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void OnSceneInitialized() override;

		void OnRainEvent(Bool enabled);

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		Bool is_supported;
		Bool rain_enabled;
		std::unique_ptr<GfxComputePipelineState> rain_drops_pso;
		TextureHandle noise_texture_handle;

	private:
		void CreatePSO();
	};


}