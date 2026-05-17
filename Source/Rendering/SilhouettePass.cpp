#include "SilhouettePass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "Components.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"
#include "Editor/Editor.h"

namespace adria
{
	static TAutoConsoleVariable<Int>   SilhouetteOutlineWidth("r.Silhouette.OutlineWidth", 2,    "Width of the selection silhouette outline in pixels");
	static TAutoConsoleVariable<Float> SilhouetteOutlineR("r.Silhouette.OutlineR",1.0f, "Red channel of the silhouette outline color");
	static TAutoConsoleVariable<Float> SilhouetteOutlineG("r.Silhouette.OutlineG",0.5f, "Green channel of the silhouette outline color");
	static TAutoConsoleVariable<Float> SilhouetteOutlineB("r.Silhouette.OutlineB",0.0f, "Blue channel of the silhouette outline color");

	SilhouettePass::SilhouettePass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void SilhouettePass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		RG_SCOPE(rg, "Silhouette");

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		std::vector<entt::entity> const& sel = g_Editor.GetSelectedEntities();
		ids_cb.idCount = std::min<Uint32>((Uint32)sel.size(), kMaxSelectedIds);
		for (Uint32 i = 0; i < ids_cb.idCount; ++i)
		{
			ids_cb.ids[i] = (Uint32)entt::to_integral(sel[i]);
		}

		struct SilhouettePassData
		{
			RGTextureReadOnlyId  input;
			RGTextureReadOnlyId  entity_id;
			RGTextureReadWriteId output;
		};

		rg.AddPass<SilhouettePassData>("Silhouette Pass",
			[=, this](SilhouettePassData& data, RenderGraphBuilder& builder)
			{
				data.input     = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				data.entity_id = builder.ReadTexture(RG_NAME(GBufferEntityID), ReadAccess_NonPixelShader);

				RGTextureDesc output_desc{};
				output_desc.width  = width;
				output_desc.height = height;
				output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(SilhouetteOutput), output_desc);
				data.output = builder.WriteTexture(RG_NAME(SilhouetteOutput));
			},
			[=, this](SilhouettePassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct SilhouetteConstants
				{
					Float  outline_width;
					Float  outline_r;
					Float  outline_g;
					Float  outline_b;
					Uint32 input_idx;
					Uint32 entity_id_idx;
					Uint32 output_idx;
					Uint32 _pad;
				} constants =
				{
					.outline_width      = (Float)SilhouetteOutlineWidth.Get(),
					.outline_r          = SilhouetteOutlineR.Get(),
					.outline_g          = SilhouetteOutlineG.Get(),
					.outline_b          = SilhouetteOutlineB.Get(),
					.input_idx          = ctx.GetReadOnlyTextureIndex(data.input),
					.entity_id_idx      = ctx.GetReadOnlyTextureIndex(data.entity_id),
					.output_idx         = ctx.GetReadWriteTextureIndex(data.output),
					._pad               = 0
				};

				cmd_list->SetPipelineState(pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, &ids_cb, sizeof(SilhouetteIdsCB));
				cmd_list->Dispatch(DivideAndRoundUp(width, 8), DivideAndRoundUp(height, 8), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(SilhouetteOutput));
	}

	void SilhouettePass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	Bool SilhouettePass::IsEnabled(PostProcessor const*) const
	{
		return g_Editor.IsActive() && g_Editor.IsSelectionModeEnabled() && !g_Editor.GetSelectedEntities().empty();
	}

	void SilhouettePass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Silhouette", 0))
				{
					ImGui::SliderInt("Outline Width", SilhouetteOutlineWidth.GetPtr(), 1, 8);
					Float color[3] = { SilhouetteOutlineR.Get(), SilhouetteOutlineG.Get(), SilhouetteOutlineB.Get() };
					if (ImGui::ColorEdit3("Outline Color", color))
					{
						SilhouetteOutlineR->Set(color[0]);
						SilhouetteOutlineG->Set(color[1]);
						SilhouetteOutlineB->Set(color[2]);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing
		);
	}

	void SilhouettePass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Silhouette;
		pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}
}
