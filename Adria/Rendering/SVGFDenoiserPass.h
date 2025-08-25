#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxComputePipelineState;

	class SVGFDenoiserPass
	{
	public:
		SVGFDenoiserPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~SVGFDenoiserPass();

		void AddPass(RenderGraph& rg);
		void OnResize(Uint32 w, Uint32 h);
		void Reset();
		void GUI();

		RGResourceName GetOutputName() const { return output_name; }

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		std::unique_ptr<GfxComputePipelineState> reprojection_pso;
		std::unique_ptr<GfxComputePipelineState> variance_pso;
		std::unique_ptr<GfxComputePipelineState> atrous_pso;

		std::unique_ptr<GfxTexture> history_color_texture;
		std::unique_ptr<GfxTexture> history_moments_texture;
		std::unique_ptr<GfxTexture> history_normal_depth_texture; 
		std::unique_ptr<GfxTexture> history_mesh_id_texture;

		RGResourceName output_name;
		Bool reset_history = true;

	private:
		void CreatePSOs();
		void CreateHistoryTextures();

		void AddReprojectionPass(RenderGraph& rg);
		void AddVarianceEstimationPass(RenderGraph& rg);
		void AddAtrousFilteringPass(RenderGraph& rg);
	};
}