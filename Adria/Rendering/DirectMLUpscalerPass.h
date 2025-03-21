#pragma once
#include "UpscalerPass.h"

namespace adria
{
	class GfxDevice;
	class MLDevice;
	class GfxComputePipelineState;
	class RenderGraph;
	class PostProcessor;

	class DirectMLUpscalerPass : public UpscalerPass
	{
		static constexpr Uint32 UPSAMPLE_LAYER_COUNT = 2;
		static constexpr Uint32 CONV_LAYER_COUNT = 7;
		static constexpr Uint32 INTERMEDIATE_BUFFER_COUNT = 2;

	public:
		explicit DirectMLUpscalerPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;

		std::unique_ptr<GfxComputePipelineState> tensor_to_texture_pso;
		std::unique_ptr<GfxComputePipelineState> texture_to_tensor_pso;
		Uint32 display_width;
		Uint32 display_height;
	private:

		void CreatePSOs();

		void CreateResolutionDependentResources();
	};
}