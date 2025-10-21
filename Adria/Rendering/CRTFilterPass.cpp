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
	static TAutoConsoleVariable<Float>	CRTHardScan("r.CRT.HardScan", -10.0f, "CRT Hard Scan");
	static TAutoConsoleVariable<Float>	CRTPixelHardness("r.CRT.PixelHardness", -3.0f, "CRT Pixel Hardness");
	static TAutoConsoleVariable<Float>	CRTWarpX("r.CRT.WarpX", 1.0f / 32.0f, "CRT Warp X");
	static TAutoConsoleVariable<Float>	CRTWarpY("r.CRT.WarpY", 1.0f / 24.0f, "CRT Warp Y");

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
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);
				Uint32 const base_index = table.base;

				struct CRTFilterConstants
				{
					Uint32 input_idx;
					Uint32 output_idx;
					Float  hard_scan;
					Float  pixel_hardness;
					Float  warp_x;
					Float  warp_y;
				} constants =
				{
					.input_idx = base_index, .output_idx = base_index + 1,
					.hard_scan = CRTHardScan.Get(), .pixel_hardness = CRTPixelHardness.Get(),
					.warp_x = CRTWarpX.Get(), .warp_y = CRTWarpY.Get()
				};

				cmd_list->SetPipelineState(crt_pso->Get());
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
					ImGui::SliderFloat("Hard Scan", CRTHardScan.GetPtr(), -16.0f, -8.0f);
					ImGui::SliderFloat("Pixel Hardness", CRTPixelHardness.GetPtr(), -4.0f, -2.0f);

					auto SnapToValidWarp = [](Float value)
					{
						static const Float valid_values[] = { 0.0f, 1.0f / 32.0f, 1.0f / 24.0f, 1.0f / 16.0f, 1.0f / 12.0f, 1.0f / 8.0f };
						Float closest = valid_values[0];
						Float min_diff = abs(value - closest);
						for (Int i = 1; i < 6; i++)
						{
							Float diff = abs(value - valid_values[i]);
							if (diff < min_diff)
							{
								min_diff = diff;
								closest = valid_values[i];
							}
						}
						return closest;
					};
					if (ImGui::SliderFloat("Warp X", CRTWarpX.GetPtr(), 0.0f, 1.0f / 8.0f, "%.4f")) 
					{
						CRTWarpX.AsVariable()->Set(SnapToValidWarp(CRTWarpX.Get()));
					}
					if (ImGui::SliderFloat("Warp Y", CRTWarpY.GetPtr(), 0.0f, 1.0f / 8.0f, "%.4f"))
					{
						CRTWarpY.AsVariable()->Set(SnapToValidWarp(CRTWarpY.Get()));
					}
					
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
		crt_pso = std::make_unique<GfxComputePipelineState>(gfx, compute_pso_desc);
	}

}

