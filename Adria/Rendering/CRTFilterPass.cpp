#include "CRTFilterPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Postprocessor.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<Bool>	CRT("r.CRT", false, "0 - Disabled, 1 - Enabled");
	//static TAutoConsoleVariable<Float>	SSRRayStep("r.SSR.RayStep", 1.60f, "Ray Step in SSR Ray March");
	//static TAutoConsoleVariable<Float>  SSRRayHitThreshold("r.SSR.HitThreshold", 2.0f, "Ray Hit Threshold in SSR Ray March");

	CRTFilterPass::CRTFilterPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	Bool CRTFilterPass::IsEnabled(PostProcessor const*) const
	{
		return CRT.Get();
	}

	void CRTFilterPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		RGResourceName last_resource = postprocessor->GetFinalResource();

		struct CRTFilterPassData
		{
			RGTextureReadOnlyId  input;
			RGTextureReadWriteId output;
		};

		rg.AddPass<CRTFilterPassData>("CRT Filter Pass",
			[=](CRTFilterPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc crt_output_desc{};
				crt_output_desc.width = width;
				crt_output_desc.height = height;
				crt_output_desc.format = GfxFormat::R8G8B8A8_UNORM;

				builder.DeclareTexture(RG_NAME(CRT_Output), crt_output_desc);
				data.output = builder.WriteTexture(RG_NAME(CRT_Output));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
			},
			[=](CRTFilterPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct CRTFilterConstants
				{
					Uint32 input_idx;
					Uint32 output_idx;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(crt_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);

		postprocessor->SetFinalResource(RG_NAME(CRT_Output));
	}

	void CRTFilterPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void CRTFilterPass::GUI()
	{
		QueueGUI([&]() {
			if (ImGui::TreeNodeEx("CRT Filter", 0))
			{
				ImGui::Checkbox("Enable CRT", CRT.GetPtr());
				if (CRT.Get())
				{
					//ImGui::SliderFloat("Ray Step", SSRRayStep.GetPtr(), 1.0f, 3.0f);
					//ImGui::SliderFloat("Ray Hit Threshold", SSRRayHitThreshold.GetPtr(), 0.25f, 5.0f);
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_None);
	}

	void CRTFilterPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_CrtFilter;
		crt_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

