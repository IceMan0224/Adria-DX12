#pragma once
#include "UpscalerPass.h"

namespace adria
{
#if defined(ADRIA_PLATFORM_MACOS)

	class GfxDevice;
	class RenderGraph;
	class PostProcessor;

	enum class MetalFXMode : Bool { Spatial, Temporal };
	enum class MetalFXQuality : Uint8 { Quality, Balanced, Performance, UltraPerformance };

	class MetalFXPass : public UpscalerPass
	{
	public:
		MetalFXPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~MetalFXPass();
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual Bool IsSupported() const override;
		virtual Bool NeedsJitter() const override { return mode == MetalFXMode::Temporal; }

	private:
		GfxDevice* gfx;
		Uint32 display_width, display_height, render_width, render_height;
		MetalFXMode mode = MetalFXMode::Temporal;
		MetalFXQuality quality = MetalFXQuality::Quality;
		Bool is_supported = true;
		Bool recreate_scalers = true;
		void* spatial_scaler  = nullptr;
		void* temporal_scaler = nullptr;

	private:
		void CreateScalers();
		void DestroyScalers();
		void RecreateRenderResolution();
	};

#else

	class MetalFXPass : public DummyUpscalerPass
	{
	public:
		MetalFXPass(GfxDevice*, Uint32, Uint32) {}
		virtual Bool IsSupported() const override { return false; }
	};

#endif
}
