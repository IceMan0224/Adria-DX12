#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "Graphics/GfxPipelineStatePermutations.h"

namespace adria
{
	class RenderGraph;

	enum class BlendMode : uint8
	{
		None,
		AlphaBlend,
		AdditiveBlend
	};

	class CopyToTexturePass
	{
	public:
		CopyToTexturePass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		GfxGraphicsPipelineStatePermutations<3> copy_psos;

	private:
		void CreatePSOs();
	};

	class AddTexturesPass
	{
	public:
		AddTexturesPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph,
			RGResourceName texture_dst,
			RGResourceName texture_src1, RGResourceName texture_src2, BlendMode mode = BlendMode::None);

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		GfxGraphicsPipelineStatePermutations<3> add_psos;

	private:
		void CreatePSOs();
	};
}