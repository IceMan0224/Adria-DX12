#pragma once
#if defined(ADRIA_PLATFORM_WINDOWS)
#define ADRIA_FFXCACAO_SUPPORTED
#endif

#include "RenderGraph/RenderGraphResourceName.h"
#if defined(ADRIA_FFXCACAO_SUPPORTED)
#include "FidelityFX/host/ffx_cacao.h"
#endif

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

#if defined(ADRIA_FFXCACAO_SUPPORTED)
	class FFXCACAOPass
	{
	public:
		FFXCACAOPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~FFXCACAOPass();

		void AddPass(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);

		Bool IsSupported() const { return is_supported; }

	private:
		Bool is_supported = true;
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
#else

	class FFXCACAOPass
	{
	public:
		FFXCACAOPass(GfxDevice*, Uint32, Uint32) {}
		~FFXCACAOPass() {}

		void AddPass(RenderGraph&) {}
		void GUI() {}
		void OnResize(Uint32, Uint32) {}

		Bool IsSupported() const { return false; }
	};

#endif
}
