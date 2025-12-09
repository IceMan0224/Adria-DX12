#include "ReSTIR_DI.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"

namespace adria
{
	struct ReSTIR_DI_Reservoir
	{
		Uint32 light_index;
		Uint32 uv_data;
		Float  weight_sum;
		Float  target_pdf;
		Float  M;
	};


	ReSTIR_DI::ReSTIR_DI(GfxDevice* gfx, Uint32 width, Uint32 height) : gfx(gfx), width(width), height(height)
	{
		if (!gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1))
		{
			return;
		}
		
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_ReSTIR_DI_InitialSampling;
		initial_sampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIR_DI_TemporalResampling;
		temporal_resampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIR_DI_SpatialResampling;
		spatial_resampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		return;

		CreateBuffers();
		supported = true;
	}

	void ReSTIR_DI::AddPasses(RenderGraph& rg)
	{
		if (!supported) return;
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("ReSTIR DI"))
				{
					ImGui::Checkbox("Enable", &enable);
					static Int current_resampling_mode = static_cast<Int>(resampling_mode);
					if (ImGui::Combo("Resampling mode", &current_resampling_mode, "None\0Temporal\0Spatial\0TemporalAndSpatial\0FusedTemporalSpatial", 5))
					{
						resampling_mode = static_cast<ResamplingMode>(current_resampling_mode);
					}
					ImGui::TreePop();
				}
			});
		if (!enable) return;

		AddInitialSamplingPass(rg);

		if (resampling_mode != ResamplingMode::None)
		{
			if (resampling_mode == ResamplingMode::Temporal || resampling_mode == ResamplingMode::TemporalAndSpatial) AddTemporalResamplingPass(rg);
			if (resampling_mode == ResamplingMode::Spatial  || resampling_mode == ResamplingMode::TemporalAndSpatial) AddSpatialResamplingPass(rg);
			if (resampling_mode == ResamplingMode::FusedTemporalSpatial) AddFusedTemporalSpatialResamplingPass(rg);
		}
	}

	void ReSTIR_DI::AddInitialSamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct InitialSamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGBufferReadWriteId reservoir;
		};

		rg.ImportBuffer(RG_NAME(ReSTIR_DI_Reservoir), reservoir_buffer.get());
		rg.AddPass<InitialSamplingPassData>("RESTIR DI Initial Sampling Pass",
			[=, this](InitialSamplingPassData& data, RenderGraphBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_DI_Reservoir));
			},
			[=, this](InitialSamplingPassData const& data, RenderGraphContext& ctx) 
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct InitialSamplingPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 reservoir_idx;
				} parameters = 
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.reservoir_idx = ctx.GetReadWriteBufferIndex(data.reservoir),
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(initial_sampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddTemporalResamplingPass(RenderGraph& rg)
	{
		struct TemporalResamplingPassData
		{

		};

		rg.AddPass<TemporalResamplingPassData>("ReSTIR DI Temporal Resampling Pass", 
			[=, this](TemporalResamplingPassData& data, RGBuilder& builder)
			{
				
			},
			[=, this](TemporalResamplingPassData const& data, RenderGraphContext& ctx)
			{
				
			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddSpatialResamplingPass(RenderGraph& rg)
	{
		struct SpatialResamplingPassData
		{

		};

		rg.AddPass<SpatialResamplingPassData>("ReSTIR DI Spatial Resampling Pass",
			[=, this](SpatialResamplingPassData& data, RGBuilder& builder)
			{

			},
			[=, this](SpatialResamplingPassData const& data, RenderGraphContext& ctx) 
			{

			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddFusedTemporalSpatialResamplingPass(RenderGraph& rg)
	{
		struct FusedTemporalSpatialResamplingPassData
		{

		};

		rg.AddPass<FusedTemporalSpatialResamplingPassData>("ReSTIR DI Fused Temporal Spatial Resampling Pass",
			[=, this](FusedTemporalSpatialResamplingPassData& data, RGBuilder& builder)
			{

			},
			[=, this](FusedTemporalSpatialResamplingPassData const& data, RenderGraphContext& ctx) 
			{

			}, RGPassType::Compute);
	}

	void ReSTIR_DI::CreateBuffers()
	{
		if (prev_reservoir_buffer == nullptr || reservoir_buffer == nullptr)
		{
			GfxBufferDesc reservoir_buffer_desc = StructuredBufferDesc<ReSTIR_DI_Reservoir>(width * height, true, false);
			prev_reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
			reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
		}
	}

}

