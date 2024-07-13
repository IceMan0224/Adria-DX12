#pragma once
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class GBufferPass
	{
	public:
		GBufferPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

		void OnRainEvent(bool enabled)
		{
			use_rain_pso = enabled;
		}

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width, height;
		bool use_rain_pso = false;
		GraphicsPipelineStatePermutations<5> gbuffer_psos;

	private:
		void CreatePSOs();
	};
}