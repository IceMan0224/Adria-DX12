#pragma once

namespace adria
{
	class GfxDevice;
	class GfxPipelineState;
	class GfxStateObject;

	enum class GfxPipelineStateID : uint8
	{
		Sky,
		MinimalAtmosphereSky,
		HosekWilkieSky,
		Texture,
		Solid_Wireframe,
		Sun,
		Ambient,
		DeferredLighting,
		VolumetricLighting,
		TiledDeferredLighting,
		ClusteredDeferredLighting,
		ClusterBuilding,
		ClusterCulling,
		ToneMap,
		FXAA,
		TAA,
		SSAO,
		HBAO,
		SSR,
		GodRays,
		FilmEffects,
		LensFlare,
		LensFlare2,
		DOF,
		ExponentialHeightFog,
		MotionBlur,
		Blur_Horizontal,
		Blur_Vertical,
		BokehGenerate,
		Bokeh,
		GenerateMips,
		MotionVectors,
		FFT_Horizontal,
		FFT_Vertical,
		InitialSpectrum,
		OceanNormals,
		Phase,
		Spectrum,
		Ocean,
		Ocean_Wireframe,
		OceanLOD,
		OceanLOD_Wireframe,
		Picking,
		BuildHistogram,
		HistogramReduction,
		Exposure,
		RTAOFilter,
		ReSTIRGI_InitialSampling,
		Unknown
	};

	namespace PSOCache
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		GfxPipelineState* Get(GfxPipelineStateID);
	};
}