#pragma once
#include "Graphics/GfxRayTracingShaderTable.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxDevice;
	class GfxShaderKey;
	class GfxStateObject;
	class SVGFDenoiserPass;

	class PathTracingPass
	{
	public:
		PathTracingPass(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height);
		~PathTracingPass();
		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);
		Bool IsSupported() const;
		void Reset();
		void GUI();

		RGResourceName GetFinalOutput() const;

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;
		Bool is_supported;

		std::unique_ptr<GfxStateObject> path_tracing_so;
		std::unique_ptr<GfxStateObject> path_tracing_svgf_enabled_so;
		std::unique_ptr<GfxGraphicsPipelineState> pt_gbuffer_pso;

		std::unique_ptr<GfxTexture> accumulation_texture = nullptr;
		Int32 accumulated_frames = 1;

		std::unique_ptr<SVGFDenoiserPass> svgf_denoiser_pass;
		Bool denoiser_active = false;

	private:
		void CreatePSOs();
		void CreateStateObjects();
		GfxStateObject* CreateStateObjectCommon(GfxShaderKey const&);
		void OnLibraryRecompiled(GfxShaderKey const&);

		void AddPTGBufferPass(RenderGraph&);
		void AddPathTracingPass(RenderGraph&);

		void CreateAccumulationTexture();
	};
}