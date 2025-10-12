#pragma once
#include "BlurPass.h"
#include "TextureHandle.h"
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class GfxDevice;
	class GfxTexture;
	class RenderGraph;

	class NNAOPass
	{
	public:
	public:
		NNAOPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~NNAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> nnao_pso;
		std::vector<TextureHandle> F_texture_handles;
		BlurPass blur_pass;

	private:
		void CreatePSO();
	};

}