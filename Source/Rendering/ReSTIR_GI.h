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

	class ReSTIR_GI
	{
	public:
		ReSTIR_GI(GfxDevice* gfx, Uint32 width, Uint32 height);
		~ReSTIR_GI();

		void AddPasses(RenderGraph& rg);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		Bool IsSupported() const { return supported; }
		void InvalidateHistory() { history_valid = false; }

	private:
		GfxDevice* gfx;
		Uint32 width, height;

		Bool supported = false;
		Bool history_valid = false;

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
		void AddOutputPass(RenderGraph& rg, Bool spatial_ran);

		void CreateBuffers();
	};
}
