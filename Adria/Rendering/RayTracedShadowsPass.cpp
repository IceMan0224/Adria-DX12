#include "RayTracedShadowsPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxRayTracingPipeline.h"
#include "Graphics/GfxRayTracingShaderBindings.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{
	RayTracedShadowsPass::RayTracedShadowsPass(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), width(width), height(height), use_inline_rt(false), is_supported(false)
	{
		if (gfx->GetCapabilities().SupportsInlineRayTracing())
		{
			is_supported = true;
			use_inline_rt = true;
		}
		else if (gfx->GetCapabilities().SupportsHardwareRayTracing())
		{
			is_supported = true;
			use_inline_rt = false;
		}
		if (IsSupported())
		{
			CreateStateObject();
			if (use_inline_rt)
			{
				ShaderManager::GetShaderRecompiledEvent().AddMember(&RayTracedShadowsPass::OnShaderRecompiled, *this);
			}
			else
			{
				ShaderManager::GetLibraryRecompiledEvent().AddMember(&RayTracedShadowsPass::OnLibraryRecompiled, *this);
			}
		}
	}
	RayTracedShadowsPass::~RayTracedShadowsPass() = default;

	void RayTracedShadowsPass::AddPass(RenderGraph& rg, Uint32 light_index)
	{
		if (!IsSupported())
		{
			return;
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct RayTracedShadowsPassData
		{
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<RayTracedShadowsPassData>("Ray Traced Shadows Pass",
			[=, this](RayTracedShadowsPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=, this](RayTracedShadowsPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct RayTracedShadowsConstants
				{
					Uint32  depth_idx;
					Uint32  light_idx;
				} constants =
				{
					.depth_idx = ctx.GetReadOnlyTextureIndex(data.depth),
					.light_idx = light_index
				};

				if (use_inline_rt)
				{
					cmd_list->SetPipelineState(ray_traced_shadows_compute_pso->Get());
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					cmd_list->Dispatch(DivideAndRoundUp(width, 16u), DivideAndRoundUp(height, 16u), 1);
				}
				else
				{
					GfxRayTracingShaderBindings* bindings = cmd_list->BeginRayTracingShaderBindings(ray_traced_shadows_pso.get());
					ADRIA_ASSERT(bindings != nullptr);
					bindings->SetRayGenShader("RTS_RayGen");
					GfxShaderGroupHandle miss_handle = bindings->AddMissShader("RTS_Miss");
					ADRIA_ASSERT(miss_handle.IsValid());
					GfxShaderGroupHandle hit_handle = bindings->AddHitGroup("ShadowAnyHitGroup");
					ADRIA_ASSERT(hit_handle.IsValid());
					bindings->Commit();

					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					cmd_list->SetRayTracingTLAS(frame_data.tlas);
					cmd_list->DispatchRays(width, height);
				}

			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void RayTracedShadowsPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	Bool RayTracedShadowsPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedShadowsPass::CreateStateObject()
	{
		if (use_inline_rt)
		{
			GfxComputePipelineStateDesc compute_pso_desc{};
			compute_pso_desc.CS = CS_RayTracedShadows;
			ray_traced_shadows_compute_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
		}
		else
		{
			GfxShader const* rt_shader = &SM_GetGfxShader(LIB_Shadows);

			GfxRayTracingPipelineDesc rt_pipeline_desc{};
			rt_pipeline_desc.max_payload_size = sizeof(Bool32);
			rt_pipeline_desc.max_attribute_size = 8;
			rt_pipeline_desc.max_recursion_depth = 1;
			rt_pipeline_desc.global_root_signature = GfxRootSignatureID::Common;

			GfxRayTracingShaderLibrary library(rt_shader,
			{
				"RTS_RayGen",
				"RTS_Miss",
				"RTS_AnyHit"
			});
			rt_pipeline_desc.libraries.push_back(library);

			GfxRayTracingHitGroup shadow_hit_group = GfxRayTracingHitGroup::Triangle(
				"ShadowAnyHitGroup",
				"",
				"RTS_AnyHit"
			);
			rt_pipeline_desc.hit_groups.push_back(shadow_hit_group);

			ray_traced_shadows_pso = gfx->CreateRayTracingPipeline(rt_pipeline_desc);
			ADRIA_ASSERT(ray_traced_shadows_pso != nullptr);
			ADRIA_ASSERT(ray_traced_shadows_pso->IsValid());
		}
	}

	void RayTracedShadowsPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_Shadows)
		{
			CreateStateObject();
		}
	}

	void RayTracedShadowsPass::OnShaderRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == CS_RayTracedShadows)
		{
			CreateStateObject();
		}
	}
}
