#pragma once
#include "FidelityFX/host/ffx_cas.h"
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class D3D12_FFXCASPass : public PostEffect
	{
	public:
		D3D12_FFXCASPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~D3D12_FFXCASPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual Bool IsGUIVisible(PostProcessor const*) const override;

	private:
		Char name_version[16] = {};
		GfxDevice* gfx;
		Uint32 width, height;
		FfxInterface* ffx_interface;
		FfxCasContextDescription cas_context_desc{};
		FfxCasContext            cas_context{};
		Float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
	};
}