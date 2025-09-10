#include "SVGFDenoiserPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Components.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"


namespace adria
{
	
	static TAutoConsoleVariable<Int>   SVGF_AtrousIterations("r.SVGF.Atrous.Iterations", 4, "Number of a-trous filter iterations.");
	static TAutoConsoleVariable<Float> SVGF_Alpha("r.SVGF.Alpha", 0.05f, "Temporal feedback factor for color.");
	static TAutoConsoleVariable<Float> SVGF_MomentsAlpha("r.SVGF.Moments.Alpha", 0.1f, "Temporal feedback factor for moments.");
	static TAutoConsoleVariable<Float> SVGF_PhiColor("r.SVGF.Phi.Color", 8.0f, "Edge-stopping function parameter for color.");
	static TAutoConsoleVariable<Float> SVGF_PhiNormal("r.SVGF.Phi.Normal", 64.0f, "Edge-stopping function parameter for normals.");
	static TAutoConsoleVariable<Float> SVGF_PhiDepth("r.SVGF.Phi.Depth", 0.05f, "Edge-stopping function parameter for depth.");
	static TAutoConsoleVariable<Float> SVGF_PhiAlbedo("r.SVGF.Phi.Albedo", 2.0f, "Edge-stopping function parameter for albedo.");

	SVGFDenoiserPass::SVGFDenoiserPass(GfxDevice* gfx, Uint32 w, Uint32 h)
		: gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
		OnResize(width, height);
	}

	SVGFDenoiserPass::~SVGFDenoiserPass() = default;

	void SVGFDenoiserPass::AddPass(RenderGraph& rg)
	{
		RG_SCOPE(rg, "SVGF Denoiser");

		rg.ImportTexture(RG_NAME(SVGF_History_DirectIllum), history_direct_illum_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_IndirectIllum), history_indirect_illum_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_Moments), history_moments_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_Length), history_length_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_NormalDepth), history_normal_depth_texture.get());

		AddReprojectionPass(rg);
		AddFilterMomentsPass(rg);
		AddAtrousPass(rg);

		rg.ExportTexture(final_direct_illum_name_for_history, history_direct_illum_texture.get());
		rg.ExportTexture(final_indirect_illum_name_for_history, history_indirect_illum_texture.get());
		rg.ExportTexture(RG_NAME(SVGF_Reprojected_Moments), history_moments_texture.get());
		rg.ExportTexture(RG_NAME(SVGF_Output_HistoryLength), history_length_texture.get());
		rg.ExportTexture(RG_NAME(SVGF_Output_NormalDepth), history_normal_depth_texture.get());

		GUI_DebugTexture("SVGF Direct Illum", history_direct_illum_texture.get());
		GUI_DebugTexture("SVGF Indirect Illum", history_indirect_illum_texture.get());
		GUI_DebugTexture("SVGF Reprojected Moments", history_moments_texture.get());
		GUI_DebugTexture("SVGF Output NormalDepth", history_normal_depth_texture.get());
	}

	void SVGFDenoiserPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
		CreateHistoryTextures();
		reset_history = true;
	}

	void SVGFDenoiserPass::Reset()
	{
		reset_history = true;
	}

	void SVGFDenoiserPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("SVGF Settings", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Temporal Alpha", SVGF_Alpha.GetPtr(), 0.0f, 1.0f);
					ImGui::SliderFloat("Moments Alpha", SVGF_MomentsAlpha.GetPtr(), 0.0f, 1.0f);
					ImGui::SliderInt("Atrous Iterations", SVGF_AtrousIterations.GetPtr(), 1, 8);
					ImGui::SliderFloat("Phi Color", SVGF_PhiColor.GetPtr(), 1.0f, 64.0f);
					ImGui::SliderFloat("Phi Normal", SVGF_PhiNormal.GetPtr(), 8.0f, 256.0f);
					ImGui::SliderFloat("Phi Depth", SVGF_PhiDepth.GetPtr(), 0.001f, 0.2f, "%.3f");
					ImGui::SliderFloat("Phi Albedo", SVGF_PhiAlbedo.GetPtr(), 1.0f, 16.0f);
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer, GUICommandSubGroup_None);
	}

	void SVGFDenoiserPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_SVGF_Reprojection;
		reprojection_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_SVGF_FilterMoments;
		filter_moments_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_SVGF_Atrous;
		atrous_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void SVGFDenoiserPass::CreateHistoryTextures()
	{
		GfxTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		desc.initial_state = GfxResourceState::ComputeUAV;

		desc.format = GfxFormat::R16G16B16A16_FLOAT;
		history_direct_illum_texture = gfx->CreateTexture(desc);
		history_indirect_illum_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R16_FLOAT;
		history_length_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R32G32_FLOAT;
		history_moments_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R32G32_UINT;
		history_normal_depth_texture = gfx->CreateTexture(desc);
	}

	void SVGFDenoiserPass::AddReprojectionPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.ImportTexture(RG_NAME(SVGF_History_DirectIllum), history_direct_illum_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_IndirectIllum), history_indirect_illum_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_Moments), history_moments_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_NormalDepth), history_normal_depth_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_Length), history_length_texture.get());

		struct ReprojectionPassData
		{
			RGTextureReadOnlyId direct_illum, indirect_illum, motion_vectors, compact_norm_depth;
			RGTextureReadOnlyId history_direct, history_indirect, history_moments, history_normal_depth, history_length;
			RGTextureReadWriteId output_direct, output_indirect, output_moments, output_normal_depth, output_history_length;
		};

		rg.AddPass<ReprojectionPassData>("SVGF Reprojection Pass",
			[=](ReprojectionPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Reprojected_Direct), desc);
				builder.DeclareTexture(RG_NAME(SVGF_Reprojected_Indirect), desc);

				desc.format = GfxFormat::R32G32_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Reprojected_Moments), desc);
				desc.format = GfxFormat::R32G32_UINT;
				builder.DeclareTexture(RG_NAME(SVGF_Output_NormalDepth), desc);
				desc.format = GfxFormat::R16_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Output_HistoryLength), desc);

				data.direct_illum = builder.ReadTexture(RG_NAME(PT_DirectRadiance), ReadAccess_NonPixelShader);
				data.indirect_illum = builder.ReadTexture(RG_NAME(PT_IndirectRadiance), ReadAccess_NonPixelShader);
				data.motion_vectors = builder.ReadTexture(RG_NAME(PT_GBuffer_MotionVectors), ReadAccess_NonPixelShader);
				data.compact_norm_depth = builder.ReadTexture(RG_NAME(PT_GBuffer_CompactNormDepth), ReadAccess_NonPixelShader);

				data.history_direct = builder.ReadTexture(RG_NAME(SVGF_History_DirectIllum), ReadAccess_NonPixelShader);
				data.history_indirect = builder.ReadTexture(RG_NAME(SVGF_History_IndirectIllum), ReadAccess_NonPixelShader);
				data.history_moments = builder.ReadTexture(RG_NAME(SVGF_History_Moments), ReadAccess_NonPixelShader);
				data.history_normal_depth = builder.ReadTexture(RG_NAME(SVGF_History_NormalDepth), ReadAccess_NonPixelShader);
				data.history_length = builder.ReadTexture(RG_NAME(SVGF_History_Length), ReadAccess_NonPixelShader);

				data.output_direct = builder.WriteTexture(RG_NAME(SVGF_Reprojected_Direct));
				data.output_indirect = builder.WriteTexture(RG_NAME(SVGF_Reprojected_Indirect));
				data.output_moments = builder.WriteTexture(RG_NAME(SVGF_Reprojected_Moments));
				data.output_normal_depth = builder.WriteTexture(RG_NAME(SVGF_Output_NormalDepth));
				data.output_history_length = builder.WriteTexture(RG_NAME(SVGF_Output_HistoryLength));
			},
			[=](ReprojectionPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.direct_illum), ctx.GetReadOnlyTexture(data.indirect_illum),
					ctx.GetReadOnlyTexture(data.motion_vectors), ctx.GetReadOnlyTexture(data.compact_norm_depth),

					ctx.GetReadOnlyTexture(data.history_direct), ctx.GetReadOnlyTexture(data.history_indirect),
					ctx.GetReadOnlyTexture(data.history_moments), ctx.GetReadOnlyTexture(data.history_normal_depth),
					ctx.GetReadOnlyTexture(data.history_length),

					ctx.GetReadWriteTexture(data.output_direct), ctx.GetReadWriteTexture(data.output_indirect),
					ctx.GetReadWriteTexture(data.output_moments), ctx.GetReadWriteTexture(data.output_normal_depth),
					ctx.GetReadWriteTexture(data.output_history_length)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct ReprojectionPassConstants
				{
					Bool32 reset;
					Float alpha;
					Float moments_alpha;
					Uint32 direct_illum_idx;
					Uint32 indirect_illum_idx;
					Uint32 motion_idx;
					Uint32 compact_norm_depth_idx;
					Uint32 history_direct_idx;
					Uint32 history_indirect_idx;
					Uint32 history_moments_idx;
					Uint32 history_normal_depth_idx;
					Uint32 history_length_idx;
					Uint32 output_direct_idx;
					Uint32 output_indirect_idx;
					Uint32 output_moments_idx;
					Uint32 output_normal_depth_idx;
					Uint32 output_history_length_idx;
				} constants =
				{
					.reset = reset_history, .alpha = SVGF_Alpha.Get(), .moments_alpha = SVGF_MomentsAlpha.Get(),
					.direct_illum_idx = i + 0, .indirect_illum_idx = i + 1, .motion_idx = i + 2, .compact_norm_depth_idx = i + 3,
					.history_direct_idx = i + 4, .history_indirect_idx = i + 5, .history_moments_idx = i + 6, .history_normal_depth_idx = i + 7, .history_length_idx = i + 8,
					.output_direct_idx = i + 9, .output_indirect_idx = i + 10, .output_moments_idx = i + 11, .output_normal_depth_idx = i + 12, .output_history_length_idx = i + 13
				};
				reset_history = false;

				cmd_list->SetPipelineState(reprojection_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}
	void SVGFDenoiserPass::AddFilterMomentsPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct FilterMomentsData
		{
			RGTextureReadOnlyId direct_illum, indirect_illum, moments, history_length, compact_norm_depth;
			RGTextureReadWriteId output_direct, output_indirect;
		};

		rg.AddPass<FilterMomentsData>("SVGF Filter Moments Pass",
			[=](FilterMomentsData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(SVGF_Filtered_Direct), desc);
				builder.DeclareTexture(RG_NAME(SVGF_Filtered_Indirect), desc);

				data.direct_illum = builder.ReadTexture(RG_NAME(SVGF_Reprojected_Direct), ReadAccess_NonPixelShader);
				data.indirect_illum = builder.ReadTexture(RG_NAME(SVGF_Reprojected_Indirect), ReadAccess_NonPixelShader);
				data.moments = builder.ReadTexture(RG_NAME(SVGF_Reprojected_Moments), ReadAccess_NonPixelShader);
				data.history_length = builder.ReadTexture(RG_NAME(SVGF_Output_HistoryLength), ReadAccess_NonPixelShader);
				data.compact_norm_depth = builder.ReadTexture(RG_NAME(PT_GBuffer_CompactNormDepth), ReadAccess_NonPixelShader);

				data.output_direct = builder.WriteTexture(RG_NAME(SVGF_Filtered_Direct));
				data.output_indirect = builder.WriteTexture(RG_NAME(SVGF_Filtered_Indirect));
			},
			[=](FilterMomentsData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.direct_illum), ctx.GetReadOnlyTexture(data.indirect_illum),
					ctx.GetReadOnlyTexture(data.moments), ctx.GetReadOnlyTexture(data.history_length),
					ctx.GetReadOnlyTexture(data.compact_norm_depth),
					ctx.GetReadWriteTexture(data.output_direct), ctx.GetReadWriteTexture(data.output_indirect)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct FilterMomentsConstants
				{
					Float phi_color;
					Float phi_normal;
					Uint32 direct_illum_idx;
					Uint32 indirect_illum_idx;
					Uint32 moments_idx;
					Uint32 history_length_idx;
					Uint32 compact_norm_depth_idx;
					Uint32 output_direct_idx;
					Uint32 output_indirect_idx;
				} constants =
				{
					.phi_color = SVGF_PhiColor.Get(), .phi_normal = SVGF_PhiNormal.Get(),
					.direct_illum_idx = i, .indirect_illum_idx = i + 1, .moments_idx = i + 2,
					.history_length_idx = i + 3, .compact_norm_depth_idx = i + 4,
					.output_direct_idx = i + 5, .output_indirect_idx = i + 6
				};

				cmd_list->SetPipelineState(filter_moments_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}
	void SVGFDenoiserPass::AddAtrousPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		Int const atrous_iterations = SVGF_AtrousIterations.Get();

		if (atrous_iterations <= 0)
		{
			final_direct_illum_name_for_history = RG_NAME(SVGF_Filtered_Direct);
			final_indirect_illum_name_for_history = RG_NAME(SVGF_Filtered_Indirect);

			output_name = RG_NAME(PT_Denoised);
			struct PassData { RGTextureReadOnlyId direct, indirect, albedo_d, albedo_i; RGTextureReadWriteId output; };
			rg.AddPass<PassData>("SVGF Modulation Only Pass",
				[&](PassData& data, RenderGraphBuilder& builder) 
				{
					builder.DeclareTexture(RG_NAME(PT_Denoised), { .width = width, .height = height, .format = GfxFormat::R16G16B16A16_FLOAT });
					data.direct = builder.ReadTexture(RG_NAME(SVGF_Filtered_Direct));
					data.indirect = builder.ReadTexture(RG_NAME(SVGF_Filtered_Indirect));
					data.albedo_d = builder.ReadTexture(RG_NAME(PT_DirectAlbedo));
					data.albedo_i = builder.ReadTexture(RG_NAME(PT_IndirectAlbedo));
					data.output = builder.WriteTexture(RG_NAME(PT_Denoised));
				},
				[=](PassData const& data, RenderGraphContext& ctx) 
				{
					GfxDevice* gfx = ctx.GetDevice();
					GfxCommandList* cmd_list = ctx.GetCommandList();
					ADRIA_TODO();
				}, RGPassType::Compute);
			return;
		}

		std::vector<RGResourceName> direct_chain(atrous_iterations + 1);
		std::vector<RGResourceName> indirect_chain(atrous_iterations + 1);

		direct_chain[0] = RG_NAME(SVGF_Filtered_Direct);
		indirect_chain[0] = RG_NAME(SVGF_Filtered_Indirect);

		struct AtrousPassData
		{
			RGTextureReadOnlyId direct_in, indirect_in, history_length, compact_norm_depth, direct_albedo, indirect_albedo;
			RGTextureReadWriteId direct_out, indirect_out;
			RGTextureReadWriteId feedback_direct_out, feedback_indirect_out;
		};

		for (Int i = 0; i < atrous_iterations; ++i)
		{
			RGResourceName direct_input = direct_chain[i];
			RGResourceName indirect_input = indirect_chain[i];

			RGResourceName direct_output = RG_NAME_IDX(SVGF_Atrous_Direct, i);
			RGResourceName indirect_output = RG_NAME_IDX(SVGF_Atrous_Indirect, i);

			direct_chain[i + 1] = direct_output;
			indirect_chain[i + 1] = indirect_output;

			Bool const is_final_iteration = (i == atrous_iterations - 1);
			if (is_final_iteration)
			{
				direct_output = RG_NAME(PT_Denoised);
			}

			std::string pass_name = "SVGF Atrous Filtering Pass " + std::to_string(i);
			rg.AddPass<AtrousPassData>(pass_name.c_str(),
				[=](AtrousPassData& data, RenderGraphBuilder& builder)
				{
					RGTextureDesc desc{};
					desc.width = width;
					desc.height = height;
					desc.format = GfxFormat::R16G16B16A16_FLOAT;

					builder.DeclareTexture(direct_output, desc);
					if (!is_final_iteration)
					{
						builder.DeclareTexture(indirect_output, desc);
					}
					else
					{
						builder.DeclareTexture(RG_NAME(SVGF_Feedback_Direct), desc);
						builder.DeclareTexture(RG_NAME(SVGF_Feedback_Indirect), desc);
					}

					data.direct_in = builder.ReadTexture(direct_input, ReadAccess_NonPixelShader);
					data.indirect_in = builder.ReadTexture(indirect_input, ReadAccess_NonPixelShader);
					data.history_length = builder.ReadTexture(RG_NAME(SVGF_Output_HistoryLength), ReadAccess_NonPixelShader);
					data.compact_norm_depth = builder.ReadTexture(RG_NAME(PT_GBuffer_CompactNormDepth), ReadAccess_NonPixelShader);
					data.direct_albedo = builder.ReadTexture(RG_NAME(PT_DirectAlbedo), ReadAccess_NonPixelShader);
					data.indirect_albedo = builder.ReadTexture(RG_NAME(PT_IndirectAlbedo), ReadAccess_NonPixelShader);

					data.direct_out = builder.WriteTexture(direct_output);
					if (!is_final_iteration)
					{
						data.indirect_out = builder.WriteTexture(indirect_output);
					}
					else
					{
						data.feedback_direct_out = builder.WriteTexture(RG_NAME(SVGF_Feedback_Direct));
						data.feedback_indirect_out = builder.WriteTexture(RG_NAME(SVGF_Feedback_Indirect));
					}
				},
				[=](AtrousPassData const& data, RenderGraphContext& ctx)
				{
					GfxDevice* gfx = ctx.GetDevice();
					GfxCommandList* cmd_list = ctx.GetCommandList();

					std::vector<GfxDescriptor> src_descriptors;
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.direct_in));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.indirect_in));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.history_length));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.compact_norm_depth));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.direct_albedo));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.indirect_albedo));
					src_descriptors.push_back(ctx.GetReadWriteTexture(data.direct_out));

					if (!is_final_iteration)
					{
						src_descriptors.push_back(ctx.GetReadWriteTexture(data.indirect_out));
					}
					else
					{
						src_descriptors.push_back(ctx.GetReadWriteTexture(data.feedback_direct_out));
						src_descriptors.push_back(ctx.GetReadWriteTexture(data.feedback_indirect_out));
					}

					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU((Uint32)src_descriptors.size());
					gfx->CopyDescriptors(dst_descriptor, src_descriptors);
					Uint32 const base_index = dst_descriptor.GetIndex();

					struct AtrousPassConstants
					{
						Int32 step_size;
						Bool32 perform_modulation;
						Float phi_color;
						Float phi_normal;
						Float phi_depth;
						Uint32 direct_in_idx;
						Uint32 indirect_in_idx;
						Uint32 history_length_idx;
						Uint32 compact_norm_depth_idx;
						Uint32 direct_albedo_idx;
						Uint32 indirect_albedo_idx;
						Uint32 direct_out_idx;
						Uint32 indirect_out_idx;
						Uint32 feedback_direct_out_idx;
						Uint32 feedback_indirect_out_idx;
					} constants =
					{
						.step_size = 1 << i, .perform_modulation = is_final_iteration,
						.phi_color = SVGF_PhiColor.Get(), .phi_normal = SVGF_PhiNormal.Get(), .phi_depth = SVGF_PhiDepth.Get(),
						.direct_in_idx = base_index, .indirect_in_idx = base_index + 1, .history_length_idx = base_index + 2, .compact_norm_depth_idx = base_index + 3,
						.direct_albedo_idx = base_index + 4, .indirect_albedo_idx = base_index + 5, .direct_out_idx = base_index + 6,
						.indirect_out_idx = is_final_iteration ? 0 : (base_index + 7),
						.feedback_direct_out_idx = is_final_iteration ? (base_index + 7) : 0,
						.feedback_indirect_out_idx = is_final_iteration ? (base_index + 8) : 0
					};

					cmd_list->SetPipelineState(atrous_pso.get());
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootCBV(2, constants);
					cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
				}, RGPassType::Compute);
		}

		output_name = RG_NAME(PT_Denoised);
		final_direct_illum_name_for_history = RG_NAME(SVGF_Feedback_Direct);
		final_indirect_illum_name_for_history = RG_NAME(SVGF_Feedback_Indirect);
	}
}