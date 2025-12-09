#include "RendererDebugViewPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<Int> DebugView("r.DebugView", (Int)RendererDebugView::Final, "Which debug view should renderer display, if any. See enum class RendererDebugView for all possible values");

	RendererDebugViewPass::RendererDebugViewPass(GfxDevice* gfx, Uint32 width, Uint32 height) : gfx(gfx), width(width), height(height)
	{
		CreatePSOs();
		DebugView->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) 
			{ 
				debug_view = (RendererDebugView)cvar->GetInt(); 
				debug_view_changed_event.Broadcast(debug_view);
			}));
	}

	RendererDebugViewPass::~RendererDebugViewPass() {}

	void RendererDebugViewPass::AddPass(RenderGraph& rg)
	{
		ADRIA_ASSERT(debug_view != RendererDebugView::Final);

		struct RendererDebugViewPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  gbuffer_custom;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadOnlyId  motion_vectors;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<RendererDebugViewPassData>("Renderer Debug View Pass",
			[=, this](RendererDebugViewPassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(RG_NAME(FinalTexture));
				data.gbuffer_normal = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.gbuffer_custom = builder.ReadTexture(RG_NAME(GBufferCustom), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(AmbientOcclusion)))
				{
					data.ambient_occlusion = builder.ReadTexture(RG_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				}
				else
				{
					data.ambient_occlusion.Invalidate();
				}

				if (builder.IsTextureDeclared(RG_NAME(VelocityBuffer)))
				{
					data.motion_vectors = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				}
				else
				{
					data.motion_vectors.Invalidate();
				}
			},
			[=, this](RendererDebugViewPassData const& data, RenderGraphContext& context)
			{
				GfxDevice* gfx = context.GetDevice();
				GfxCommandList* cmd_list = context.GetCommandList();
				
				const Float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearTexture(context.GetTexture(*data.output), clear);

				struct RendererDebugViewIndices
				{
					Uint32 normal_metallic_idx;
					Uint32 diffuse_idx;
					Uint32 depth_idx;
					Uint32 emissive_idx;
					Uint32 custom_idx;
					Uint32 ao_idx;
					Uint32 motion_vectors_idx;
					Uint32 output_idx;
				} indices =
				{
					.normal_metallic_idx = context.GetReadOnlyTextureIndex(data.gbuffer_normal),
					.diffuse_idx = context.GetReadOnlyTextureIndex(data.gbuffer_albedo),
					.depth_idx = context.GetReadOnlyTextureIndex(data.depth),
					.emissive_idx = context.GetReadOnlyTextureIndex(data.gbuffer_emissive),
					.custom_idx = context.GetReadOnlyTextureIndex(data.gbuffer_custom),
					.ao_idx = data.ambient_occlusion.IsValid() ? context.GetReadOnlyTextureIndex(data.ambient_occlusion) : GfxCommon::GetCommonViewBindlessIndex(GfxCommonViewType::WhiteTexture2D_SRV),
					.motion_vectors_idx = data.motion_vectors.IsValid() ? context.GetReadOnlyTextureIndex(data.motion_vectors) : GfxCommon::GetCommonViewBindlessIndex(GfxCommonViewType::BlackTexture2D_SRV),
					.output_idx = context.GetReadWriteTextureIndex(data.output)
				};

				struct RendererDebugViewConstants
				{
					Float triangle_overdraw_scale;
				} constants = 
				{
					.triangle_overdraw_scale = (Float)triangle_overdraw_scale
				};

				static std::array<Char const*, (Uint32)RendererDebugView::Count> OutputDefines =
				{
					"",
					"OUTPUT_DIFFUSE",
					"OUTPUT_NORMALS",
					"OUTPUT_DEPTH",
					"OUTPUT_ROUGHNESS",
					"OUTPUT_METALLIC",
					"OUTPUT_EMISSIVE",
					"OUTPUT_MATERIAL_ID",
					"OUTPUT_MESHLET_ID",
					"OUTPUT_AO",
					"OUTPUT_INDIRECT",
					"OUTPUT_SHADING_EXTENSION",
					"OUTPUT_CUSTOM",
					"OUTPUT_MIPMAPS",
					"OUTPUT_OVERDRAW",
					"OUTPUT_MOTION_VECTORS"
				};
				renderer_output_psos->AddDefine(OutputDefines[(Uint32)debug_view], "1");

				cmd_list->SetPipelineState(renderer_output_psos->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, indices);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void RendererDebugViewPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Renderer Output Settings", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Triangle Overdraw Scale", &triangle_overdraw_scale, 0.1f, 10.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer);
	}

	void RendererDebugViewPass::SetDebugView(RendererDebugView value)
	{
		DebugView->Set((Int)value);
	}

	void RendererDebugViewPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_RendererDebugView;
		renderer_output_psos = std::make_unique<GfxComputePipelineStatePermutations>(gfx, compute_pso_desc);
	}

}