#pragma once
#include "FidelityFX/host/ffx_cacao.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class D3D12_FFXCACAOPass
	{
	public:
		D3D12_FFXCACAOPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~D3D12_FFXCACAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);

	private:
		Char name_version[16] = {};
		GfxDevice* gfx;
		Uint32 width, height;
		FfxInterface*			   ffx_interface;
		Int32					   preset_id = 0;
		Bool                       use_downsampled_ssao = false;
		Bool                       generate_normals = false;
		FfxCacaoSettings		   cacao_settings{};
		FfxCacaoContextDescription cacao_context_desc{};
		FfxCacaoContext            cacao_context{};
		FfxCacaoContext            cacao_downsampled_context{};
		Bool                       context_created = false;

	private:
		void CreateContext();
		void DestroyContext();
	};
}