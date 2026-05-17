#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Graphics/GfxPipelineStateFwd.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxStateObject;
	class GfxShaderKey;
	class GfxRayTracingPipeline;

	class RayTracedShadowsPass
	{
	public:
		RayTracedShadowsPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RayTracedShadowsPass();
		void AddPass(RenderGraph& rendergraph, Uint32 light_index);
		void OnResize(Uint32 w, Uint32 h);

		Bool IsSupported() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxRayTracingPipeline> ray_traced_shadows_pso;
		std::unique_ptr<GfxComputePipelineState> ray_traced_shadows_compute_pso;
		Uint32 width, height;
		Bool is_supported;
		Bool use_inline_rt;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
		void OnShaderRecompiled(GfxShaderKey const&);
	};
}
