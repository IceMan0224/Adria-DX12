#pragma once
#if defined(ADRIA_PLATFORM_WINDOWS)
#define ADRIA_DLSS3_SUPPORTED
#endif

#include "UpscalerPass.h"
#include "Utilities/Delegate.h"
#if defined(ADRIA_DLSS3_SUPPORTED)
#include "nvsdk_ngx_defs.h"
#endif


struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class RenderGraph;

#if defined(ADRIA_DLSS3_SUPPORTED)
	class DLSS3Pass : public UpscalerPass
	{
	public:
		DLSS3Pass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~DLSS3Pass();

		virtual void OnResize(Uint32 w, Uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_create = true;
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

		NVSDK_NGX_Parameter* ngx_parameters = nullptr;
		NVSDK_NGX_Handle* dlss_feature = nullptr;
		Bool needs_create = true;

		NVSDK_NGX_PerfQuality_Value perf_quality = NVSDK_NGX_PerfQuality_Value_Balanced;
		Float sharpness = 0.5f;

	private:
		Bool InitializeNVSDK_NGX();
		void RecreateRenderResolution();

		void CreateDLSS(GfxCommandList* cmd_list);
		void ReleaseDLSS();
	};
#else

	class DLSS3Pass : public DummyUpscalerPass
	{
    public:
        DLSS3Pass(GfxDevice* gfx, Uint32 w, Uint32 h) {}
	};

#endif
}
