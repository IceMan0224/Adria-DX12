#include "TAAPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	static TAutoConsoleVariable<Bool> TAA("r.TAA", false, "Enable or Disable TAA");

	TAAPass::TAAPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void TAAPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct TAAPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId history;
			RGTextureReadOnlyId velocity;
			RGTextureReadWriteId output;
		};

		rg.AddPass<TAAPassData>("TAA Pass",
			[=](TAAPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc taa_desc{};
				taa_desc.width = width;
				taa_desc.height = height;
				taa_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(TAAOutput), taa_desc);
				data.output = builder.WriteTexture(RG_NAME(TAAOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_PixelShader);
				data.history = builder.ReadTexture(RG_NAME(HistoryBuffer), ReadAccess_PixelShader);
				data.velocity = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](TAAPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadOnlyTexture(data.history),
					ctx.GetReadOnlyTexture(data.velocity),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

				struct TAAConstants
				{
					Uint32 scene_idx;
					Uint32 prev_scene_idx;
					Uint32 velocity_idx;
					Uint32 output_idx;
				} constants =
				{
					.scene_idx = table, .prev_scene_idx = table + 1, .velocity_idx = table + 2, .output_idx = table + 3
				};

				cmd_list->SetPipelineState(taa_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(TAAOutput));
	}

	Bool TAAPass::IsEnabled(PostProcessor const* postprocessor) const
	{
		return TAA.Get() && !postprocessor->HasUpscaler();
	}

	void TAAPass::GUI()
	{
		QueueGUI([&]()
			{
				ImGui::Checkbox("TAA", TAA.GetPtr());
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Antialiasing);
	}

	Bool TAAPass::IsGUIVisible(PostProcessor const* postprocessor) const
	{
		return !postprocessor->HasUpscaler();
	}

	void TAAPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void TAAPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Taa;
		taa_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}
}