#include "BlurPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"


namespace adria
{

	BlurPass::BlurPass(GfxDevice* gfx) : gfx(gfx)
	{
		CreatePSOs();
	}

	BlurPass::~BlurPass() = default;

	void BlurPass::AddPass(RenderGraph& rendergraph, RGResourceName src_texture, RGResourceName blurred_texture,
		Char const* pass_name)
	{

		static Uint64 counter = 0;
		counter++;

		std::string horizontal_name = "Horizontal Blur Pass " + std::string(pass_name);
		std::string vertical_name = "Vertical Blur Pass " + std::string(pass_name);
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		struct BlurPassData
		{
			RGTextureReadOnlyId src_texture;
			RGTextureReadWriteId dst_texture;
		};

		rendergraph.AddPass<BlurPassData>(horizontal_name.c_str(),
			[=, this](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc = builder.GetTextureDesc(src_texture);

				builder.DeclareTexture(RG_NAME_IDX(Intermediate, counter), blur_desc);
				data.dst_texture = builder.WriteTexture(RG_NAME_IDX(Intermediate, counter));
				data.src_texture = builder.ReadTexture(src_texture, ReadAccess_NonPixelShader);
			},
			[=, this](BlurPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxTextureDesc const& src_desc = ctx.GetTexture(*data.src_texture).GetDesc();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.src_texture),
					ctx.GetReadWriteTexture(data.dst_texture)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

				struct BlurConstants
				{
					Uint32 input_idx;
					Uint32 output_idx;
				} constants =
				{
					.input_idx = table, .output_idx = table + 1
				};

				cmd_list->SetPipelineState(blur_horizontal_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(src_desc.width, 512), src_desc.height, 1);
			}, async_compute ? RGPassType::AsyncCompute : RGPassType::Compute, RGPassFlags::None);

		rendergraph.AddPass<BlurPassData>(vertical_name.c_str(),
			[=, this](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc = builder.GetTextureDesc(src_texture);
				builder.DeclareTexture(blurred_texture, blur_desc);
				data.dst_texture = builder.WriteTexture(blurred_texture);
				data.src_texture = builder.ReadTexture(RG_NAME_IDX(Intermediate, counter), ReadAccess_NonPixelShader);
			},
			[=, this](BlurPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxTextureDesc const& src_desc = ctx.GetTexture(*data.src_texture).GetDesc();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.src_texture),
					ctx.GetReadWriteTexture(data.dst_texture)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

				struct BlurConstants
				{
					Uint32 input_idx;
					Uint32 output_idx;
				} constants =
				{
					.input_idx = table, .output_idx = table + 1
				};

				cmd_list->SetPipelineState(blur_vertical_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(src_desc.width, DivideAndRoundUp(src_desc.height, 512), 1);

			}, async_compute ? RGPassType::AsyncCompute : RGPassType::Compute, RGPassFlags::None);

	}

	void BlurPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Blur_Horizontal;
		blur_horizontal_pso = std::make_unique<GfxComputePipelineState>(gfx, compute_pso_desc);

		compute_pso_desc.CS = CS_Blur_Vertical;
		blur_vertical_pso = std::make_unique<GfxComputePipelineState>(gfx, compute_pso_desc);
	}

}