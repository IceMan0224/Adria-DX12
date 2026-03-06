#pragma once
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "entt/entity/fwd.hpp"


namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class RenderGraph;

	class ReSTIR_DI
	{
		enum class ResamplingMode : Uint8
		{
			None = 0,
			Temporal = 1,
			Spatial = 2,
			TemporalAndSpatial = 3,
			FusedTemporalSpatial = 4
		};

	public:
		ReSTIR_DI(GfxDevice* gfx, Uint32 width, Uint32 height);

		void AddPasses(RenderGraph& rg);
		void GUI();
		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
			CreateBuffers();
		}
		Bool IsSupported() const { return supported; }
		Bool IsEnabled() const { return supported && enable; }

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		Bool supported = false;
		Bool enable = false;
		Bool history_valid = false;
		ResamplingMode resampling_mode = ResamplingMode::TemporalAndSpatial;
		Float max_temporal_M = 20.0f;
		Float temporal_depth_threshold = 0.1f;
		Float temporal_normal_threshold = 0.5f;
		Uint32 spatial_sample_count = 5;
		Float spatial_radius = 30.0f;

		std::unique_ptr<GfxBuffer>  prev_reservoir_buffer;
		std::unique_ptr<GfxBuffer>	reservoir_buffer;

		std::unique_ptr<GfxComputePipelineState> initial_sampling_pso;
		std::unique_ptr<GfxComputePipelineState> temporal_resampling_pso;
		std::unique_ptr<GfxComputePipelineState> spatial_resampling_pso;
		std::unique_ptr<GfxComputePipelineState> output_pso;

	private:
		void AddInitialSamplingPass(RenderGraph& rg);
		void AddTemporalResamplingPass(RenderGraph& rg);
		void AddSpatialResamplingPass(RenderGraph& rg);
		void AddFusedTemporalSpatialResamplingPass(RenderGraph& rg);
		void AddOutputPass(RenderGraph& rg, Bool spatial_ran);

		void CreateBuffers();
	};
}