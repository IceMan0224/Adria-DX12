#pragma once
#include "PostEffect.h"
#include "FidelityFX/host/ffx_vrs.h"
#include "Graphics/GfxShadingRate.h"
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "RenderGraph/RenderGraphResourceName.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;
	class GfxTexture;
	class PostProcessor;

	class D3D12_FFXVRSPass : public PostEffect
	{
	public:
		D3D12_FFXVRSPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~D3D12_FFXVRSPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void GUI() override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual Bool IsSupported() const override { return is_supported; }

	private:
		Char name_version[16] = {};
		GfxDevice* gfx;
		Uint32 width, height;

		FfxInterface*			 ffx_interface;
		FfxVrsContextDescription vrs_context_description;
		FfxVrsContext            vrs_context;
		Bool                     context_created = false;

		std::unique_ptr<GfxTexture> vrs_image;
		GfxDescriptor vrs_image_srv;
		std::unique_ptr<GfxGraphicsPipelineState> vrs_overlay_pso;

		Bool is_supported = false;
		Bool  additional_shading_rates_supported = false;
		Uint32 shading_rate_image_tile_size;

	private:
		void CreateVRSImage();
		void CreateOverlayPSO();
		void DestroyContext();
		void CreateContext();
	};
}