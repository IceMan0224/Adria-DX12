#include "ReSTIR_GI.h"
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
	enum class GIResamplingMode : Uint8
	{
		None = 0,
		Temporal = 1,
		Spatial = 2,
		TemporalAndSpatial = 3
	};

	static TAutoConsoleVariable<Int>   ReSTIR_GI_ResamplingMode("r.ReSTIR_GI.ResamplingMode", (Int)GIResamplingMode::TemporalAndSpatial, "Resampling mode for ReSTIR GI: 0 - None, 1 - Temporal, 2 - Spatial, 3 - TemporalAndSpatial");
	static TAutoConsoleVariable<Float> ReSTIR_GI_MaxTemporalM("r.ReSTIR_GI.MaxTemporalM", 20.0f, "Maximum temporal M clamp value for ReSTIR GI");
	static TAutoConsoleVariable<Float> ReSTIR_GI_TemporalDepthThreshold("r.ReSTIR_GI.TemporalDepthThreshold", 0.1f, "Temporal depth rejection threshold for ReSTIR GI");
	static TAutoConsoleVariable<Float> ReSTIR_GI_MaxReservoirAge("r.ReSTIR_GI.MaxReservoirAge", 30.0f, "Maximum reservoir age in frames before temporal rejection for ReSTIR GI");
	static TAutoConsoleVariable<Int>   ReSTIR_GI_SpatialSampleCount("r.ReSTIR_GI.SpatialSampleCount", 5, "Number of spatial neighbour samples for ReSTIR GI");
	static TAutoConsoleVariable<Float> ReSTIR_GI_SpatialRadius("r.ReSTIR_GI.SpatialRadius", 30.0f, "Spatial sampling search radius for ReSTIR GI");
	static TAutoConsoleVariable<Float> ReSTIR_GI_MaxSpatialM("r.ReSTIR_GI.MaxSpatialM", 100.0f, "Maximum M clamp after spatial reuse for ReSTIR GI");

	struct ReSTIR_GI_Reservoir
	{
		Float  position[3];
		Float  weight_sum;
		Float  normal[3];
		Float  target_pdf;
		Float  radiance[3];
		Float  M;
		Float  age;
		Float  pad0;
		Float  pad1;
		Float  pad2;
	};


	ReSTIR_GI::ReSTIR_GI(GfxDevice* gfx, Uint32 width, Uint32 height) : gfx(gfx), width(width), height(height)
	{
		if (!gfx->GetCapabilities().SupportsInlineRayTracing())
		{
			return;
		}

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_ReSTIR_GI_InitialSampling;
		initial_sampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIR_GI_TemporalResampling;
		temporal_resampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIR_GI_SpatialResampling;
		spatial_resampling_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_ReSTIR_GI_Output;
		output_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		CreateBuffers();
		supported = true;
	}

	ReSTIR_GI::~ReSTIR_GI() = default;

	void ReSTIR_GI::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
		prev_reservoir_buffer.reset();
		reservoir_buffer.reset();
		CreateBuffers();
		history_valid = false;
	}

	void ReSTIR_GI::GUI()
	{
		if (!supported)
		{
			return;
		}

		QueueGUI([&]()
			{
				if (ImGui::TreeNode("ReSTIR GI"))
				{
					ImGui::Combo("Resampling mode", ReSTIR_GI_ResamplingMode.GetPtr(), "None\0Temporal\0Spatial\0TemporalAndSpatial\0", 4);
					ImGui::SliderFloat("Max Temporal M", ReSTIR_GI_MaxTemporalM.GetPtr(), 1.0f, 50.0f);
					ImGui::SliderFloat("Temporal Depth Threshold", ReSTIR_GI_TemporalDepthThreshold.GetPtr(), 0.01f, 0.5f);
					ImGui::SliderFloat("Max Reservoir Age", ReSTIR_GI_MaxReservoirAge.GetPtr(), 1.0f, 100.0f);
					ImGui::SliderInt("Spatial Samples", ReSTIR_GI_SpatialSampleCount.GetPtr(), 1, 16);
					ImGui::SliderFloat("Spatial Radius", ReSTIR_GI_SpatialRadius.GetPtr(), 1.0f, 100.0f);
					ImGui::SliderFloat("Max Spatial M", ReSTIR_GI_MaxSpatialM.GetPtr(), 1.0f, 500.0f);
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer, GUICommandSubGroup_Lighting);
	}

	void ReSTIR_GI::AddPasses(RenderGraph& rg)
	{
		if (!supported)
		{
			history_valid = false;
			return;
		}
		RG_SCOPE(rg, "ReSTIR GI");

		rg.ImportBuffer(RG_NAME(ReSTIR_GI_Reservoir), reservoir_buffer.get());
		rg.ImportBuffer(RG_NAME(ReSTIR_GI_PrevReservoir), prev_reservoir_buffer.get());

		AddInitialSamplingPass(rg);
		GIResamplingMode resampling_mode = static_cast<GIResamplingMode>(ReSTIR_GI_ResamplingMode.Get());
		Bool uses_temporal = (resampling_mode == GIResamplingMode::Temporal || resampling_mode == GIResamplingMode::TemporalAndSpatial);
		Bool uses_spatial  = (resampling_mode == GIResamplingMode::Spatial  || resampling_mode == GIResamplingMode::TemporalAndSpatial);
		if (resampling_mode != GIResamplingMode::None)
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

	void ReSTIR_GI::AddInitialSamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct InitialSamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGBufferReadWriteId reservoir;
		};

		rg.AddPass<InitialSamplingPassData>("ReSTIR GI Initial Sampling Pass",
			[=, this](InitialSamplingPassData& data, RenderGraphBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_GI_Reservoir));
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

	void ReSTIR_GI::AddTemporalResamplingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct TemporalResamplingPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId albedo;
			RGTextureReadOnlyId prev_depth;
			RGBufferReadWriteId reservoir;
			RGBufferReadOnlyId  prev_reservoir;
		};

		rg.AddPass<TemporalResamplingPassData>("ReSTIR GI Temporal Resampling Pass",
			[=, this](TemporalResamplingPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.prev_depth = builder.ReadTexture(RG_NAME(DepthHistory));
				data.reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_GI_Reservoir));
				data.prev_reservoir = builder.ReadBuffer(RG_NAME(ReSTIR_GI_PrevReservoir));
			},
			[=, this](TemporalResamplingPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxBuffer& reservoir = ctx.GetBuffer(*data.reservoir);
				cmd_list->BufferBarrier(reservoir, GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
				cmd_list->FlushBarriers();

				struct TemporalResamplingPassParameters
				{
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 prev_depth_idx;
					Uint32 reservoir_idx;
					Uint32 prev_reservoir_idx;
					Float  max_temporal_M;
					Float  depth_threshold;
					Float  max_reservoir_age;
				} parameters =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.prev_depth_idx = ctx.GetReadOnlyTextureIndex(data.prev_depth),
					.reservoir_idx = ctx.GetReadWriteBufferIndex(data.reservoir),
					.prev_reservoir_idx = ctx.GetReadOnlyBufferIndex(data.prev_reservoir),
					.max_temporal_M = ReSTIR_GI_MaxTemporalM.Get(),
					.depth_threshold = ReSTIR_GI_TemporalDepthThreshold.Get(),
					.max_reservoir_age = ReSTIR_GI_MaxReservoirAge.Get(),
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(temporal_resampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_GI::AddSpatialResamplingPass(RenderGraph& rg)
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

		rg.AddPass<SpatialResamplingPassData>("ReSTIR GI Spatial Resampling Pass",
			[=, this](SpatialResamplingPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.input_reservoir = builder.ReadBuffer(RG_NAME(ReSTIR_GI_Reservoir));
				data.output_reservoir = builder.WriteBuffer(RG_NAME(ReSTIR_GI_PrevReservoir));
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
					Float  max_spatial_M;
				} parameters =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.normal_idx = ctx.GetReadOnlyTextureIndex(data.normal),
					.albedo_idx = ctx.GetReadOnlyTextureIndex(data.albedo),
					.input_reservoir_idx = ctx.GetReadOnlyBufferIndex(data.input_reservoir),
					.output_reservoir_idx = ctx.GetReadWriteBufferIndex(data.output_reservoir),
					.spatial_sample_count = (Uint32)ReSTIR_GI_SpatialSampleCount.Get(),
					.spatial_radius = ReSTIR_GI_SpatialRadius.Get(),
					.max_spatial_M = ReSTIR_GI_MaxSpatialM.Get(),
				};
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetPipelineState(spatial_resampling_pso->Get());
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void ReSTIR_GI::AddOutputPass(RenderGraph& rg, Bool spatial_ran)
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

		rg.AddPass<OutputPassData>("ReSTIR GI Output Pass",
			[=, this](OutputPassData& data, RenderGraphBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.final_reservoir = builder.ReadBuffer(spatial_ran ? RG_NAME(ReSTIR_GI_PrevReservoir) : RG_NAME(ReSTIR_GI_Reservoir));

				RGTextureDesc output_desc{};
				output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				output_desc.width = width;
				output_desc.height = height;
				builder.DeclareTexture(RG_NAME(ReSTIR_GI_Output), output_desc);
				data.output = builder.WriteTexture(RG_NAME(ReSTIR_GI_Output));
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

	void ReSTIR_GI::CreateBuffers()
	{
		if (prev_reservoir_buffer == nullptr || reservoir_buffer == nullptr)
		{
			GfxBufferDesc reservoir_buffer_desc = StructuredBufferDesc<ReSTIR_GI_Reservoir>(width * height, true, false);
			prev_reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
			reservoir_buffer = gfx->CreateBuffer(reservoir_buffer_desc);
		}
	}

}
