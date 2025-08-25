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
	
	static TAutoConsoleVariable<Int>   SVGF_AtrousIterations("r.SVGF.Atrous.Iterations", 5, "Number of a-trous filter iterations.");
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

		AddReprojectionPass(rg);
		
		rg.ExportTexture(RG_NAME(SVGF_Output_NormalDepth), history_normal_depth_texture.get());
		rg.ExportTexture(RG_NAME(SVGF_Output_MeshID), history_mesh_id_texture.get());

		AddVarianceEstimationPass(rg);
		AddAtrousFilteringPass(rg);
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
			}, GUICommandGroup_PathTracer, GUICommandSubGroup_Denoising);
	}

	void SVGFDenoiserPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_SVGF_Reprojection;
		reprojection_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_SVGF_Variance;
		variance_pso = gfx->CreateComputePipelineState(compute_pso_desc);

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
		history_color_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R32G32_FLOAT;
		history_moments_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R32G32_UINT;
		history_normal_depth_texture = gfx->CreateTexture(desc);

		desc.format = GfxFormat::R32_UINT;
		history_mesh_id_texture = gfx->CreateTexture(desc);
	}

	void SVGFDenoiserPass::AddReprojectionPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.ImportTexture(RG_NAME(SVGF_History_Color), history_color_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_Moments), history_moments_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_NormalDepth), history_normal_depth_texture.get());
		rg.ImportTexture(RG_NAME(SVGF_History_MeshID), history_mesh_id_texture.get());


		struct ReprojectionPassData
		{
			RGTextureReadOnlyId noisy_input, motion_vectors, depth, normal, albedo, mesh_id;
			RGTextureReadOnlyId history_color, history_moments, history_normal_depth, history_mesh_id;
			RGTextureReadWriteId output_color, output_moments, output_normal_depth, output_mesh_id;
		};

		rg.AddPass<ReprojectionPassData>("SVGF Reprojection Pass",
			[=](ReprojectionPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Reprojected_Color), desc);
				desc.format = GfxFormat::R32G32_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Reprojected_Moments), desc);
				desc.format = GfxFormat::R32G32_UINT;
				builder.DeclareTexture(RG_NAME(SVGF_Output_NormalDepth), desc);
				desc.format = GfxFormat::R32_UINT;
				builder.DeclareTexture(RG_NAME(SVGF_Output_MeshID), desc);

				data.noisy_input = builder.ReadTexture(RG_NAME(PT_Output), ReadAccess_NonPixelShader);
				data.motion_vectors = builder.ReadTexture(RG_NAME(PT_MotionVectors), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(PT_Depth), ReadAccess_NonPixelShader);
				data.normal = builder.ReadTexture(RG_NAME(PT_Normal), ReadAccess_NonPixelShader);
				data.albedo = builder.ReadTexture(RG_NAME(PT_Albedo), ReadAccess_NonPixelShader);
				data.mesh_id = builder.ReadTexture(RG_NAME(PT_MeshID), ReadAccess_NonPixelShader);

				data.history_color = builder.ReadTexture(RG_NAME(SVGF_History_Color), ReadAccess_NonPixelShader);
				data.history_moments = builder.ReadTexture(RG_NAME(SVGF_History_Moments), ReadAccess_NonPixelShader);
				data.history_normal_depth = builder.ReadTexture(RG_NAME(SVGF_History_NormalDepth), ReadAccess_NonPixelShader);
				data.history_mesh_id = builder.ReadTexture(RG_NAME(SVGF_History_MeshID), ReadAccess_NonPixelShader);

				data.output_color = builder.WriteTexture(RG_NAME(SVGF_Reprojected_Color));
				data.output_moments = builder.WriteTexture(RG_NAME(SVGF_Reprojected_Moments));
				data.output_normal_depth = builder.WriteTexture(RG_NAME(SVGF_Output_NormalDepth));
				data.output_mesh_id = builder.WriteTexture(RG_NAME(SVGF_Output_MeshID));
			},
			[=](ReprojectionPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.noisy_input), ctx.GetReadOnlyTexture(data.motion_vectors),
					ctx.GetReadOnlyTexture(data.depth), ctx.GetReadOnlyTexture(data.normal),
					ctx.GetReadOnlyTexture(data.albedo), ctx.GetReadOnlyTexture(data.mesh_id),
					ctx.GetReadOnlyTexture(data.history_color), ctx.GetReadOnlyTexture(data.history_moments),
					ctx.GetReadOnlyTexture(data.history_normal_depth), ctx.GetReadOnlyTexture(data.history_mesh_id),
					ctx.GetReadWriteTexture(data.output_color), ctx.GetReadWriteTexture(data.output_moments),
					ctx.GetReadWriteTexture(data.output_normal_depth), ctx.GetReadWriteTexture(data.output_mesh_id)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct ReprojectionPassConstants
				{
					Bool32 reset;
					Float alpha;
					Float moments_alpha;
					Float phi_albedo_rejection;
					Uint32 input_idx;
					Uint32 motion_idx;
					Uint32 depth_idx;
					Uint32 normal_idx;
					Uint32 albedo_idx;
					Uint32 mesh_id_idx;
					Uint32 history_color_idx;
					Uint32 history_moments_idx;
					Uint32 history_normal_depth_idx;
					Uint32 history_mesh_id_idx;
					Uint32 output_color_idx;
					Uint32 output_moments_idx;
					Uint32 output_normal_depth_idx;
					Uint32 output_mesh_id_idx;
				} constants =
				{
					.reset = reset_history, .alpha = SVGF_Alpha.Get(), .moments_alpha = SVGF_MomentsAlpha.Get(), .phi_albedo_rejection = 0.2f,
					.input_idx = i, .motion_idx = i + 1, .depth_idx = i + 2, .normal_idx = i + 3, .albedo_idx = i + 4, .mesh_id_idx = i + 5,
					.history_color_idx = i + 6, .history_moments_idx = i + 7, .history_normal_depth_idx = i + 8, .history_mesh_id_idx = i + 9,
					.output_color_idx = i + 10, .output_moments_idx = i + 11, .output_normal_depth_idx = i + 12, .output_mesh_id_idx = i + 13
				};
				reset_history = false;

				cmd_list->SetPipelineState(reprojection_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);

	}

	void SVGFDenoiserPass::AddVarianceEstimationPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct VarianceEstimationPassData
		{
			RGTextureReadOnlyId color;
			RGTextureReadOnlyId moments;
			RGTextureReadWriteId output_color;
			RGTextureReadWriteId output_moments;
		};
		rg.AddPass<VarianceEstimationPassData>("SVGF Variance Estimation Pass",
			[=](VarianceEstimationPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(SVGF_Ping), desc);

				data.color = builder.ReadTexture(RG_NAME(SVGF_Reprojected_Color), ReadAccess_NonPixelShader);
				data.moments = builder.ReadTexture(RG_NAME(SVGF_Reprojected_Moments), ReadAccess_NonPixelShader);
				data.output_color = builder.WriteTexture(RG_NAME(SVGF_Ping));
				data.output_moments = builder.WriteTexture(RG_NAME(SVGF_History_Moments));
			},
			[=](VarianceEstimationPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.color), ctx.GetReadOnlyTexture(data.moments),
					ctx.GetReadWriteTexture(data.output_color), ctx.GetReadWriteTexture(data.output_moments)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct VariancePassDataConstants
				{
					Uint32 color_idx;
					Uint32 moments_idx;
					Uint32 output_color_idx;
					Uint32 output_moments_idx;
				}
				constants = { .color_idx = i, .moments_idx = i + 1, .output_color_idx = i + 2, .output_moments_idx = i + 3 };
				cmd_list->SetPipelineState(variance_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);

			}, RGPassType::Compute);

	}

	void SVGFDenoiserPass::AddAtrousFilteringPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct AtrousPassData
		{
			RGTextureReadOnlyId input, moments, normal, depth, albedo, mesh_id;
			RGTextureReadWriteId output;
			RGTextureReadWriteId history_color_update;
		};

		RGResourceName AtrousArguments[] = { RG_NAME(SVGF_Ping), RG_NAME(SVGF_Pong) };

		Int const atrous_iterations = SVGF_AtrousIterations.Get();
		for (Int i = 0; i < atrous_iterations; ++i)
		{
			RGResourceName atrous_input = AtrousArguments[i % 2];
			RGResourceName atrous_output = AtrousArguments[(i + 1) % 2];

			std::string pass_name = "SVGF Atrous Filtering Pass " + std::to_string(i);
			rg.AddPass<AtrousPassData>(pass_name.c_str(),
				[=](AtrousPassData& data, RenderGraphBuilder& builder)
				{
					data.input = builder.ReadTexture(atrous_input, ReadAccess_NonPixelShader);
					data.moments = builder.ReadTexture(RG_NAME(SVGF_History_Moments), ReadAccess_NonPixelShader);
					data.normal = builder.ReadTexture(RG_NAME(PT_Normal), ReadAccess_NonPixelShader);
					data.depth = builder.ReadTexture(RG_NAME(PT_Depth), ReadAccess_NonPixelShader);
					data.albedo = builder.ReadTexture(RG_NAME(PT_Albedo), ReadAccess_NonPixelShader);
					data.mesh_id = builder.ReadTexture(RG_NAME(PT_MeshID), ReadAccess_NonPixelShader);

					if (i == 0)
					{
						RGTextureDesc desc{};
						desc.width = width;
						desc.height = height;
						desc.format = GfxFormat::R16G16B16A16_FLOAT;
						builder.DeclareTexture(RG_NAME(SVGF_Pong), desc);

						data.history_color_update = builder.WriteTexture(RG_NAME(SVGF_History_Color));
					}
					data.output = builder.WriteTexture(atrous_output);
				},
				[=](AtrousPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					std::vector<GfxDescriptor> src_descriptors;
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.input));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.moments));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.normal));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.depth));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.albedo));
					src_descriptors.push_back(ctx.GetReadOnlyTexture(data.mesh_id));
					src_descriptors.push_back(ctx.GetReadWriteTexture(data.output));
					if (i == 0) src_descriptors.push_back(ctx.GetReadWriteTexture(data.history_color_update));

					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU((Uint32)src_descriptors.size());
					gfx->CopyDescriptors(dst_descriptor, src_descriptors);
					Uint32 const base_index = dst_descriptor.GetIndex();

					struct AtrousPassConstants
					{
						Int32 step_size;
						Float phi_color;
						Float phi_normal;
						Float phi_depth;
						Float phi_albedo;
						Uint32 input_idx;
						Uint32 moments_idx;
						Uint32 normal_idx;
						Uint32 depth_idx;
						Uint32 albedo_idx;
						Uint32 mesh_id_idx;
						Uint32 output_idx;
						Uint32 history_color_update_idx;
					} constants =
					{
						.step_size = 1 << i, .phi_color = SVGF_PhiColor.Get(), .phi_normal = SVGF_PhiNormal.Get(),
						.phi_depth = SVGF_PhiDepth.Get(), .phi_albedo = SVGF_PhiAlbedo.Get(),
						.input_idx = base_index, .moments_idx = base_index + 1, .normal_idx = base_index + 2, .depth_idx = base_index + 3,
						.albedo_idx = base_index + 4, .mesh_id_idx = base_index + 5, .output_idx = base_index + 6,
						.history_color_update_idx = (i == 0) ? (base_index + 7) : 0
					};

					cmd_list->SetPipelineState(atrous_pso.get());
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootCBV(2, constants);
					cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
				}, RGPassType::Compute);
		}

		output_name = AtrousArguments[atrous_iterations % 2];
	}

}