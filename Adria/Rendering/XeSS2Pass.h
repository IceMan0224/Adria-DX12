#pragma once
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"
#if defined(ADRIA_PLATFORM_WINDOWS)
#include "XeSS/xess.h"
#endif

namespace adria
{
	class GfxDevice;
	class RenderGraph;

#if defined(ADRIA_PLATFORM_WINDOWS)
	class XeSS2Pass : public UpscalerPass
	{
	public:
		XeSS2Pass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~XeSS2Pass();

		virtual void OnResize(Uint32 w, Uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_init = true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		Char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		Uint32 display_width, display_height;
		Uint32 render_width, render_height;

		xess_context_handle_t context{};
		xess_quality_settings_t quality_setting = XESS_QUALITY_SETTING_QUALITY;
		Bool needs_init = false;

	private:
		void XeSSInit();
		void RecreateRenderResolution();
	};
#else

    class XeSS2Pass : public DummyUpscalerPass
    {
    public:
        XeSS2Pass(GfxDevice*, Uint32, Uint32) {}
    };

#endif
}
