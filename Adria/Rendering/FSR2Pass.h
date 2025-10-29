#pragma once
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"
#if defined(ADRIA_PLATFORM_WINDOWS)
#include "FidelityFX/host/ffx_fsr2.h"
#endif

namespace adria
{
	class GfxDevice;
	class RenderGraph;

#if defined(ADRIA_PLATFORM_WINDOWS)
	class FSR2Pass : public UpscalerPass
	{
	public:
		FSR2Pass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~FSR2Pass();

		virtual void OnResize(Uint32 w, Uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			recreate_context = true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual Bool IsSupported() const override { return is_supported; }

	private:
		Bool is_supported = true;
		Char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		Uint32 display_width, display_height;
		Uint32 render_width, render_height;

		FfxInterface* ffx_interface;
		FfxFsr2ContextDescription fsr2_context_desc{};
		FfxFsr2Context fsr2_context{};
		Bool recreate_context = true;

		FfxFsr2QualityMode fsr2_quality_mode = FFX_FSR2_QUALITY_MODE_QUALITY;
		Float custom_upscale_ratio = 1.0f;
		Float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
#else

	class FSR2Pass : public DummyUpscalerPass
	{
    public:
        FSR2Pass(GfxDevice* gfx, Uint32 w, Uint32 h) {}
	};

#endif
}
