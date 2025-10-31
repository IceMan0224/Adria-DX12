#include "NNAOPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Packing.h"
#include "Core/ConsoleManager.h"
#include "Core/Paths.h"
#include "Editor/GUICommand.h"

namespace adria
{
	static TAutoConsoleVariable<Float> NNAORadius("r.NNAO.Radius", 1.0f, "Controls the radius of NNAO");
	static TAutoConsoleVariable<Float> NNAOPower("r.NNAO.Power", 1.5f, "Controls the radius of NNAO");

	NNAOPass::NNAOPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h), blur_pass(gfx)
	{
		CreatePSO();
	}
	NNAOPass::~NNAOPass() = default;

	void NNAOPass::AddPass(RenderGraph& rg)
	{
		RG_SCOPE(rg, "NNAO");

		struct NNAOPassData
		{
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<NNAOPassData>("NNAO Pass",
			[=, this](NNAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc nnao_desc{};
				nnao_desc.format = GfxFormat::R8_UNORM;
				nnao_desc.width = width;
				nnao_desc.height = height;

				builder.DeclareTexture(RG_NAME(NNAO_Output), nnao_desc);
				data.output = builder.WriteTexture(RG_NAME(NNAO_Output));
				data.gbuffer_normal = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[&](NNAOPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.gbuffer_normal),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

				ADRIA_ASSERT(F_texture_handles.size() == 4);
				struct NNAOConstants
				{
					Uint32	 nnao_params_packed;
					Uint32   depth_idx;
					Uint32   normal_idx;
					Uint32   output_idx;
					Uint32	 F0_idx;
					Uint32	 F1_idx;
					Uint32	 F2_idx;
					Uint32	 F3_idx;
				} constants =
				{
					.nnao_params_packed = PackTwoFloatsToUint32(NNAORadius.Get(), NNAOPower.Get()),
					.depth_idx = table, .normal_idx = table + 1, .output_idx = table + 2,
					.F0_idx = (Uint32)F_texture_handles[0], .F1_idx = (Uint32)F_texture_handles[1],
					.F2_idx = (Uint32)F_texture_handles[2], .F3_idx = (Uint32)F_texture_handles[3],
				};

				cmd_list->SetPipelineState(nnao_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::AsyncCompute);

		blur_pass.SetAsyncCompute(true);
		blur_pass.AddPass(rg, RG_NAME(NNAO_Output), RG_NAME(AmbientOcclusion), " NNAO");
	}

	void NNAOPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("NNAO", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Power",  NNAOPower.GetPtr(), 1.0f, 16.0f);
					ImGui::SliderFloat("Radius", NNAORadius.GetPtr(), 0.5f, 4.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_AO);
	}

	void NNAOPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void NNAOPass::OnSceneInitialized()
	{
		if (F_texture_handles.empty())
		{
			F_texture_handles.push_back(g_TextureManager.LoadTexture(paths::MLDir + "nnao_f0.tga"));
			F_texture_handles.push_back(g_TextureManager.LoadTexture(paths::MLDir + "nnao_f1.tga"));
			F_texture_handles.push_back(g_TextureManager.LoadTexture(paths::MLDir + "nnao_f2.tga"));
			F_texture_handles.push_back(g_TextureManager.LoadTexture(paths::MLDir + "nnao_f3.tga"));
		}
	}

	void NNAOPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Nnao;
		nnao_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

}

