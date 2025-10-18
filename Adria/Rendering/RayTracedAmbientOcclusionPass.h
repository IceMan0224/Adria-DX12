#pragma once
#include "BlurPass.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class GfxRayTracingPipeline;
	class GfxShaderKey;
	class RenderGraph;

	class RayTracedAmbientOcclusionPass
	{
		ADRIA_TODO("Move this to console variables");
		struct RTAOParams
		{
			Float radius = 2.0f;
			Float power_log = -1.0f;
			Float filter_distance_sigma = 10.0f;
			Float filter_depth_sigma = 0.25f;
		};

	public:
		RayTracedAmbientOcclusionPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RayTracedAmbientOcclusionPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		Bool IsSupported() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxRayTracingPipeline> ray_traced_ambient_occlusion_pso;
		std::unique_ptr<GfxComputePipelineState> rtao_filter_pso;
		Uint32 width, height;
		BlurPass blur_pass;

		Bool is_supported;
		RTAOParams params{};

	private:
		void CreatePSO();
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}