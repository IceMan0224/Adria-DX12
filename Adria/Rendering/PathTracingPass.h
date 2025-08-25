#pragma once
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxDevice;
	class GfxShaderKey;
	class GfxStateObject;
	class SVGFDenoiserPass;
	class GfxComputePipelineState;

	class PathTracingPass
	{
	public:
		PathTracingPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~PathTracingPass();
		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);
		Bool IsSupported() const;
		void Reset();
		void GUI();

		RGResourceName GetFinalOutput() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> path_tracing_so;
		std::unique_ptr<GfxStateObject> path_tracing_with_denoiser_so;
		std::unique_ptr<GfxComputePipelineState> remodulate_pso;
		Uint32 width, height;
		Bool is_supported;
		std::unique_ptr<GfxTexture> accumulation_texture = nullptr;
		Int32 accumulated_frames = 1;

		std::unique_ptr<SVGFDenoiserPass> svgf_denoiser_pass;
		Bool denoiser_active = false;

	private:
		void CreatePSO();
		void CreateStateObjects();
		GfxStateObject* CreateStateObjectCommon(GfxShaderKey const&);
		void OnLibraryRecompiled(GfxShaderKey const&);

		void AddPathTracingPass(RenderGraph&);
		void AddRemodulatePass(RenderGraph&);

		void CreateAccumulationTexture();
	};
}