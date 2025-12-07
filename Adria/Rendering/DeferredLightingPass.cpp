#include "DeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxCommon.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Math/Packing.h"

using namespace DirectX;

namespace adria
{

	DeferredLightingPass::DeferredLightingPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	void DeferredLightingPass::AddPass(RenderGraph& rg)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  gbuffer_custom;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<LightingPassData>("Deferred Lighting Pass",
			[=, this](LightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(HDR_RenderTarget), hdr_desc);

				data.output			  = builder.WriteTexture(RG_NAME(HDR_RenderTarget));
				data.gbuffer_normal   = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo   = builder.ReadTexture(RG_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.gbuffer_custom   = builder.ReadTexture(RG_NAME(GBufferCustom),  ReadAccess_NonPixelShader);
				data.depth			  = builder.ReadTexture(RG_NAME(DepthStencil),  ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(AmbientOcclusion)))
				{
					data.ambient_occlusion = builder.ReadTexture(RG_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				}
				else
				{
					data.ambient_occlusion.Invalidate();
				}
				for (RGResourceName shadow_texture : shadow_textures)
				{
					std::ignore = builder.ReadTexture(shadow_texture);
				}
			},
			[=, this](LightingPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				static const Float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearTexture(ctx.GetTexture(*data.output), clear);

				struct DeferredLightingConstants
				{
					Uint32 normal_metallic_idx;
					Uint32 diffuse_idx;
					Uint32 emissive_idx;
					Uint32 custom_idx;
					Uint32 depth_idx;
					Uint32 ao_idx;
					Uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = ctx.GetReadOnlyTextureIndex(data.gbuffer_normal),
					.diffuse_idx = ctx.GetReadOnlyTextureIndex(data.gbuffer_albedo),
					.emissive_idx = ctx.GetReadOnlyTextureIndex(data.gbuffer_emissive),
					.custom_idx = ctx.GetReadOnlyTextureIndex(data.gbuffer_custom),
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.ao_idx = data.ambient_occlusion.IsValid() ? ctx.GetReadOnlyTextureIndex(data.ambient_occlusion) : GfxCommon::GetCommonViewBindlessIndex(GfxCommonViewType::WhiteTexture2D_SRV),
					.output_idx = ctx.GetReadWriteTextureIndex(data.output)
				};

				cmd_list->SetPipelineState(deferred_lighting_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);

		shadow_textures.clear();
	}

	void DeferredLightingPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_DeferredLighting;
		deferred_lighting_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

}

