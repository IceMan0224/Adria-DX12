#pragma once
#include "Graphics/GfxPipelineStateFwd.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class SilhouettePass
	{
	public:
		SilhouettePass(GfxDevice* gfx, Uint32 w, Uint32 h);

		void AddPass(RenderGraph& rg, Uint32 selected_entity_id);
		void OnResize(Uint32 w, Uint32 h);
		void GUI();

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> pso;

	private:
		void CreatePSO();
	};
}
