#include "FXAAPass.h"
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
	static TAutoConsoleVariable<Bool> FXAA("r.FXAA", true, "Enable or Disable FXAA");

	FXAAPass::FXAAPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void FXAAPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct FXAAPassData
		{
			RGTextureReadWriteId	output;
			RGTextureReadOnlyId		ldr;
		};

		rg.AddPass<FXAAPassData>("FXAA Pass",
			[=, this](FXAAPassData& data, RenderGraphBuilder& builder)
			{
				data.ldr = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_NAME(FinalTexture)));
				data.output = builder.WriteTexture(RG_NAME(FinalTexture));
			},
			[=, this](FXAAPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct FXAAConstants
				{
					Uint32 depth_idx;
					Uint32 output_idx;
				} constants =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.ldr),
					.output_idx = ctx.GetReadWriteTextureIndex(data.output)
				};

				cmd_list->SetPipelineState(fxaa_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	Bool FXAAPass::IsEnabled(PostProcessor const*) const
	{
		return FXAA.Get();
	}

	void FXAAPass::GUI()
	{
		QueueGUI([&]()
			{
				ImGui::Checkbox("FXAA", FXAA.GetPtr());
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Antialiasing);
	}

	void FXAAPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void FXAAPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Fxaa;
		fxaa_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

}
