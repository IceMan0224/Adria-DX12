#include "TiledDeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Packing.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	
	TiledDeferredLightingPass::TiledDeferredLightingPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h) : reg(reg), gfx(gfx), width(w), height(h),
		add_textures_pass(gfx, width, height), copy_to_texture_pass(gfx, width, height)
	{
		CreatePSOs();
	}

	void TiledDeferredLightingPass::AddPass(RenderGraph& rendergraph)
	{
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		struct TiledDeferredLightingPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  gbuffer_custom;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadWriteId output;
			RGTextureReadWriteId debug_output;
		};

		rendergraph.AddPass<TiledDeferredLightingPassData>("Tiled Deferred Lighting Pass",
			[=](TiledDeferredLightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				hdr_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_NAME(HDR_RenderTarget), hdr_desc);
				builder.DeclareTexture(RG_NAME(TiledDebugTarget), hdr_desc);

				data.output = builder.WriteTexture(RG_NAME(HDR_RenderTarget));
				data.debug_output = builder.WriteTexture(RG_NAME(TiledDebugTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.gbuffer_custom = builder.ReadTexture(RG_NAME(GBufferCustom), ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(AmbientOcclusion)))
					data.ambient_occlusion = builder.ReadTexture(RG_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else data.ambient_occlusion.Invalidate();

				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](TiledDeferredLightingPassData const& data, RenderGraphContext& context)
			{
				GfxDevice* gfx = context.GetDevice();
				GfxCommandList* cmd_list = context.GetCommandList();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
												context.GetReadOnlyTexture(data.gbuffer_albedo),
												context.GetReadOnlyTexture(data.gbuffer_emissive),
												context.GetReadOnlyTexture(data.gbuffer_custom),
												context.GetReadOnlyTexture(data.depth),
												data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
												context.GetReadWriteTexture(data.output),
												context.GetReadWriteTexture(data.debug_output) };

				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_handles);

				struct TiledLightingConstants
				{
					Uint32 normal_idx;
					Uint32 diffuse_idx;
					Uint32 emissive_idx;
					Uint32 custom_idx;
					Uint32 depth_idx;
					Uint32 ao_idx;
					Uint32 output_idx;
					Uint32 debug_data_packed;
				} constants =
				{
					.normal_idx = table, .diffuse_idx = table + 1, .emissive_idx = table + 2, .custom_idx = table + 3,
					.depth_idx = table + 4, .ao_idx = table + 5, .output_idx = table + 6,
					.debug_data_packed = PackTwoUint16ToUint32(visualize_tiled ? Uint16(table + 7) : 0, (Uint16)visualize_max_lights)
				};

				if (visualize_tiled)
				{
					ADRIA_ASSERT(table + 7 < UINT16_MAX);
				}

				static constexpr Float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				GfxTexture const& tiled_target = context.GetTexture(*data.output);
				GfxTexture const& tiled_debug_target = context.GetTexture(*data.debug_output);
				cmd_list->ClearTexture(tiled_target, black);
				cmd_list->ClearTexture(tiled_debug_target, black);

				cmd_list->SetPipelineState(tiled_deferred_lighting_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		if (visualize_tiled)
		{
			copy_to_texture_pass.AddPass(rendergraph, RG_NAME(HDR_RenderTarget), RG_NAME(TiledDebugTarget), BlendMode::AdditiveBlend);
		}
	}

	void TiledDeferredLightingPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Tiled Deferred", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Visualize Tiles", &visualize_tiled);
					if (visualize_tiled) ImGui::SliderInt("Visualize Scale", &visualize_max_lights, 1, 32);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer
		);
	}

	void TiledDeferredLightingPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_TiledDeferredLighting;
		tiled_deferred_lighting_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

}

