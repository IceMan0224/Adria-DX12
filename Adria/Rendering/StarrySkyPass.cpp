#include "StarrySkyPass.h"
#include "ShaderStructs.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "SunManager.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{
	StarrySkyPass::StarrySkyPass(GfxDevice* gfx, Uint32 w, Uint32 h)
		: gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void StarrySkyPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct StarrySkyPassData
		{
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  input;
			RGTextureReadWriteId output;
		};

		rg.AddPass<StarrySkyPassData>("Starry Sky Pass",
			[=, this](StarrySkyPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				desc.width = width;
				desc.height = height;

				builder.DeclareTexture(RG_NAME(StarrySkyOutput), desc);
				data.output = builder.WriteTexture(RG_NAME(StarrySkyOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=, this](StarrySkyPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				Float sun_elevation_deg = asin(-g_SunManager.GetSunDirection().y) * (180.0f / 3.14159265f);
				Float intensity = 1.0f - Clamp(sun_elevation_deg / 10.0f, 0.0f, 1.0f);

				struct StarrySkyConstants
				{
					Float  stars_threshold;
					Float  stars_exposure;
					Float  stars_intensity;
					Uint32 depth_idx;
					Uint32 scene_idx;
					Uint32 output_idx;
				} constants =
				{
					.stars_threshold = stars_threshold,
					.stars_exposure = stars_exposure,
					.stars_intensity = intensity,
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.scene_idx = ctx.GetReadOnlyTextureIndex(data.input),
					.output_idx = ctx.GetReadWriteTextureIndex(data.output)
				};

				cmd_list->SetPipelineState(starry_sky_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(StarrySkyOutput));
	}

	Bool StarrySkyPass::IsEnabled(PostProcessor const*) const
	{
		Float sun_elevation_deg = asin(-g_SunManager.GetSunDirection().y) * (180.0f / 3.14159265f);
		return sun_elevation_deg < 10.0f;
	}

	void StarrySkyPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w;
		height = h;
	}

	void StarrySkyPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Starry Sky", 0))
				{
					ImGui::SliderFloat("Stars Threshold", &stars_threshold, 1.0f, 20.0f);
					ImGui::SliderFloat("Stars Exposure", &stars_exposure, 10.0f, 1000.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer, GUICommandSubGroup_Environment);
	}

	void StarrySkyPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_StarrySky;
		starry_sky_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}
}
