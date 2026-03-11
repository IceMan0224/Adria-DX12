#include "ReSTIR_DI.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	enum class ResamplingMode : Uint8
	{
		None = 0,
		Temporal = 1,
		Spatial = 2,
		TemporalAndSpatial = 3
	};

	static TAutoConsoleVariable<Bool>  ReSTIR_DI_Enable("r.ReSTIR_DI.Enable", false, "Enables or disables ReSTIR DI");
	static TAutoConsoleVariable<Int>   ReSTIR_DI_ResamplingMode("r.ReSTIR_DI.ResamplingMode", (Int)ResamplingMode::TemporalAndSpatial, "Resampling mode for ReSTIR DI: 0 - None, 1 - Temporal, 2 - Spatial, 3 - TemporalAndSpatial, 4 - FusedTemporalSpatial");
	static TAutoConsoleVariable<Float> ReSTIR_DI_MaxTemporalM("r.ReSTIR_DI.MaxTemporalM", 20.0f, "Maximum temporal M clamp value for ReSTIR DI");
	static TAutoConsoleVariable<Float> ReSTIR_DI_TemporalDepthThreshold("r.ReSTIR_DI.TemporalDepthThreshold", 0.1f, "Temporal depth rejection threshold for ReSTIR DI");
	static TAutoConsoleVariable<Float> ReSTIR_DI_TemporalNormalThreshold("r.ReSTIR_DI.TemporalNormalThreshold", 0.5f, "Temporal normal rejection threshold for ReSTIR DI");
	static TAutoConsoleVariable<Int>   ReSTIR_DI_SpatialSampleCount("r.ReSTIR_DI.SpatialSampleCount", 5, "Number of spatial neighbour samples for ReSTIR DI");
	static TAutoConsoleVariable<Float> ReSTIR_DI_SpatialRadius("r.ReSTIR_DI.SpatialRadius", 30.0f, "Spatial sampling search radius for ReSTIR DI");

	struct ReSTIR_DI_Reservoir
	{
		Uint32 light_index;
		Uint32 uv_data;
		Float  weight_sum;
		Float  target_pdf;
		Float  M;
	};


	Bool ReSTIR_DI::IsEnabled() const { return supported && ReSTIR_DI_Enable.Get(); }

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
					ImGui::Checkbox("Enable", ReSTIR_DI_Enable.GetPtr());
					ImGui::Combo("Resampling mode", ReSTIR_DI_ResamplingMode.GetPtr(), "None\0Temporal\0Spatial\0TemporalAndSpatial\0", 4);
					ImGui::SliderFloat("Max Temporal M", ReSTIR_DI_MaxTemporalM.GetPtr(), 1.0f, 50.0f);
					ImGui::SliderFloat("Temporal Depth Threshold", ReSTIR_DI_TemporalDepthThreshold.GetPtr(), 0.01f, 0.5f);
					ImGui::SliderFloat("Temporal Normal Threshold", ReSTIR_DI_TemporalNormalThreshold.GetPtr(), 0.0f, 1.0f);
					ImGui::SliderInt("Spatial Samples", ReSTIR_DI_SpatialSampleCount.GetPtr(), 1, 16);
					ImGui::SliderFloat("Spatial Radius", ReSTIR_DI_SpatialRadius.GetPtr(), 1.0f, 100.0f);
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer);
	}

	void ReSTIR_DI::AddPasses(RenderGraph& rg)
	{
		if (!supported || !ReSTIR_DI_Enable.Get())
		{
			history_valid = false;
			return;
		}

		rg.ImportBuffer(RG_NAME(ReSTIR_DI_Reservoir), reservoir_buffer.get());
		rg.ImportBuffer(RG_NAME(ReSTIR_DI_PrevReservoir), prev_reservoir_buffer.get());

		AddInitialSamplingPass(rg);
		ResamplingMode resampling_mode = static_cast<ResamplingMode>(ReSTIR_DI_ResamplingMode.Get());
		Bool uses_temporal = (resampling_mode == ResamplingMode::Temporal || resampling_mode == ResamplingMode::TemporalAndSpatial);
		Bool uses_spatial  = (resampling_mode == ResamplingMode::Spatial  || resampling_mode == ResamplingMode::TemporalAndSpatial);
		if (resampling_mode != ResamplingMode::None)
		{
			if (uses_temporal && history_valid) 
			{
				AddTemporalResamplingPass(rg);
			}
			if (uses_spatial) 
			{
				AddSpatialResamplingPass(rg);
			}
		}

		AddOutputPass(rg, uses_spatial);
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
					.max_temporal_M = ReSTIR_DI_MaxTemporalM.Get(),
					.depth_threshold = ReSTIR_DI_TemporalDepthThreshold.Get(),
					.normal_threshold = ReSTIR_DI_TemporalNormalThreshold.Get(),
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
					.spatial_sample_count = (Uint32)ReSTIR_DI_SpatialSampleCount.Get(),
					.spatial_radius = ReSTIR_DI_SpatialRadius.Get(),
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(spatial_resampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
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
