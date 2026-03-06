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
		if (!gfx->GetCapabilities().SupportsInlineRayTracing())
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

		compute_pso_desc.CS = CS_ReSTIR_DI_Output;
		output_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		CreateBuffers();
		supported = true;
	}

	void ReSTIR_DI::GUI()
	{
		if (!supported) 
		{
			return;
		}

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
					ImGui::SliderFloat("Max Temporal M", &max_temporal_M, 1.0f, 50.0f);
					ImGui::SliderFloat("Temporal Depth Threshold", &temporal_depth_threshold, 0.01f, 0.5f);
					ImGui::SliderFloat("Temporal Normal Threshold", &temporal_normal_threshold, 0.0f, 1.0f);
					ImGui::SliderInt("Spatial Samples", (Int*)&spatial_sample_count, 1, 16);
					ImGui::SliderFloat("Spatial Radius", &spatial_radius, 1.0f, 100.0f);
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer);
	}

	void ReSTIR_DI::AddPasses(RenderGraph& rg)
	{
		if (!supported || !enable)
		{
			history_valid = false;
			return;
		}

		rg.ImportBuffer(RG_NAME(ReSTIR_DI_Reservoir), reservoir_buffer.get());
		rg.ImportBuffer(RG_NAME(ReSTIR_DI_PrevReservoir), prev_reservoir_buffer.get());

		AddInitialSamplingPass(rg);

		Bool uses_temporal = (resampling_mode == ResamplingMode::Temporal || resampling_mode == ResamplingMode::TemporalAndSpatial);
		Bool uses_spatial  = (resampling_mode == ResamplingMode::Spatial  || resampling_mode == ResamplingMode::TemporalAndSpatial);

		if (resampling_mode != ResamplingMode::None)
		{
			if (uses_temporal && history_valid) AddTemporalResamplingPass(rg);
			if (uses_spatial) AddSpatialResamplingPass(rg);
			if (resampling_mode == ResamplingMode::FusedTemporalSpatial) AddFusedTemporalSpatialResamplingPass(rg);
		}

		Bool spatial_ran = uses_spatial || resampling_mode == ResamplingMode::FusedTemporalSpatial;
		AddOutputPass(rg, spatial_ran);
		if (!uses_spatial)
		{
			std::swap(prev_reservoir_buffer, reservoir_buffer);
		}
		history_valid = true;
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
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct TemporalResamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGBufferReadWriteId reservoir;
			RGBufferReadOnlyId  prev_reservoir;
		};

		rg.AddPass<TemporalResamplingPassData>("ReSTIR DI Temporal Resampling Pass",
			[=, this](TemporalResamplingPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_DI_Reservoir));
				data.prev_reservoir = builder.ReadBuffer(RG_NAME(ReSTIR_DI_PrevReservoir));
			},
			[=, this](TemporalResamplingPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct TemporalResamplingPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 reservoir_idx;
					Uint32 prev_reservoir_idx;
					Float  max_temporal_M;
					Float  depth_threshold;
					Float  normal_threshold;
				} parameters =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.reservoir_idx = ctx.GetReadWriteBufferIndex(data.reservoir),
					.prev_reservoir_idx = ctx.GetReadOnlyBufferIndex(data.prev_reservoir),
					.max_temporal_M = max_temporal_M,
					.depth_threshold = temporal_depth_threshold,
					.normal_threshold = temporal_normal_threshold,
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(temporal_resampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddSpatialResamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct SpatialResamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGBufferReadOnlyId  input_reservoir;
			RGBufferReadWriteId output_reservoir;
		};

		rg.AddPass<SpatialResamplingPassData>("ReSTIR DI Spatial Resampling Pass",
			[=, this](SpatialResamplingPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.input_reservoir = builder.ReadBuffer(RG_NAME(ReSTIR_DI_Reservoir));
				data.output_reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_DI_PrevReservoir));
			},
			[=, this](SpatialResamplingPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct SpatialResamplingPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 input_reservoir_idx;
					Uint32 output_reservoir_idx;
					Uint32 spatial_sample_count;
					Float  spatial_radius;
				} parameters =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.input_reservoir_idx = ctx.GetReadOnlyBufferIndex(data.input_reservoir),
					.output_reservoir_idx = ctx.GetReadWriteBufferIndex(data.output_reservoir),
					.spatial_sample_count = spatial_sample_count,
					.spatial_radius = spatial_radius,
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(spatial_resampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_DI::AddFusedTemporalSpatialResamplingPass(RenderGraph& rg)
	{
		// No dedicated fused shader exists; fall back to temporal + spatial
		if (history_valid) 
		{
			AddTemporalResamplingPass(rg);
		}
		AddSpatialResamplingPass(rg);
	}

	void ReSTIR_DI::AddOutputPass(RenderGraph& rg, Bool spatial_ran)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct OutputPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGBufferReadOnlyId  final_reservoir;
			RGTextureReadWriteId output;
		};

		rg.AddPass<OutputPassData>("ReSTIR DI Output Pass",
			[=, this](OutputPassData& data, RenderGraphBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.final_reservoir = builder.ReadBuffer(spatial_ran ? RG_NAME(ReSTIR_DI_PrevReservoir) : RG_NAME(ReSTIR_DI_Reservoir));

				RGTextureDesc output_desc{};
				output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				output_desc.width = width;
				output_desc.height = height;
				builder.DeclareTexture(RG_NAME(ReSTIR_DI_Output), output_desc);
				data.output = builder.WriteTexture(RG_NAME(ReSTIR_DI_Output));
			},
			[=, this](OutputPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct OutputPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 final_reservoir_idx;
					Uint32 output_idx;
				} parameters =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.final_reservoir_idx = ctx.GetReadOnlyBufferIndex(data.final_reservoir),
					.output_idx = ctx.GetReadWriteTextureIndex(data.output),
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(output_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
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
