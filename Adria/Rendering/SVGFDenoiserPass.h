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
		std::unique_ptr<GfxComputePipelineState> filter_moments_pso; 
		std::unique_ptr<GfxComputePipelineState> atrous_pso;

		std::unique_ptr<GfxTexture> history_direct_illum_texture;
		std::unique_ptr<GfxTexture> history_indirect_illum_texture;
		std::unique_ptr<GfxTexture> history_length_texture;
		std::unique_ptr<GfxTexture> history_moments_texture;
		std::unique_ptr<GfxTexture> history_normal_depth_texture;

		RGResourceName output_name;
		RGResourceName final_direct_illum_name_for_history;
		RGResourceName final_indirect_illum_name_for_history;
		Bool reset_history = true;

	private:
		void CreatePSOs();
		void CreateHistoryTextures();

		void AddReprojectionPass(RenderGraph& rg);
		void AddFilterMomentsPass(RenderGraph& rg);
		void AddAtrousPass(RenderGraph& rg);
	};
}