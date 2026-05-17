#include "RainDropsPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "Graphics/GfxDevice.h" 
#include "Graphics/GfxPipelineState.h" 
#include "Graphics/GfxCommon.h" 
#include "RenderGraph/RenderGraph.h"
#include "Core/Paths.h"

namespace adria
{
	RainDropsPass::RainDropsPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h),
		rain_enabled(false), is_supported(false), noise_texture_handle(INVALID_TEXTURE_HANDLE)
	{
		CreatePSO();
		is_supported = gfx->GetCapabilities().SupportsTypedUAVLoadAdditionalFormats();
	}

	Bool RainDropsPass::IsEnabled(PostProcessor const* postprocessor) const
	{
		return rain_enabled;
	}

	void RainDropsPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!is_supported)
		{
			return;
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct RainDropsPassData
		{
			RGTextureReadWriteId output;
		};

		rg.AddPass<RainDropsPassData>("Rain Drops Pass",
			[=, this](RainDropsPassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(postprocessor->GetFinalResource());
			},
			[=, this](RainDropsPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct RainDropsConstants
				{
					Uint32   output_idx;
					Uint32   noise_idx;
				} constants =
				{
					.output_idx = ctx.GetReadWriteTextureIndex(data.output),
					.noise_idx = g_TextureManager.GetBindlessIndex(noise_texture_handle)
				};
				cmd_list->SetPipelineState(rain_drops_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);

			}, RGPassType::Compute, RGPassFlags::None);
	}

	void RainDropsPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void RainDropsPass::OnSceneInitialized()
	{
		noise_texture_handle = g_TextureManager.LoadTexture(paths::TexturesDir + "RGBA_Noise.png");
	}

	void RainDropsPass::OnRainEvent(Bool enabled)
	{
		rain_enabled = enabled;
	}

	void RainDropsPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_RainDrops;
		rain_drops_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}
}
