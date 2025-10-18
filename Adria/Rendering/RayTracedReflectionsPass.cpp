#include "RayTracedReflectionsPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "PostProcessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxRayTracingPipeline.h"
#include "Graphics/GfxRayTracingShaderBindings.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<Bool> RTR("r.RTR", true, "0 - Disabled, 1 - Enabled");
	
	RayTracedReflectionsPass::RayTracedReflectionsPass(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), width(width), height(height), blur_pass(gfx), copy_to_texture_pass(gfx, width, height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (IsSupported())
		{
			CreateStateObject();
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&RayTracedReflectionsPass::OnLibraryRecompiled, *this);
		}
	}
	RayTracedReflectionsPass::~RayTracedReflectionsPass() = default;

	void RayTracedReflectionsPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct RayTracedReflectionsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId diffuse;
			RGTextureReadWriteId output;
		};

		rg.AddPass<RayTracedReflectionsPassData>("Ray Traced Reflections Pass",
			[=](RayTracedReflectionsPassData& data, RGBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R8G8B8A8_UNORM;
				builder.DeclareTexture(RG_NAME(RTR_OutputNoisy), desc);

				data.output = builder.WriteTexture(RG_NAME(RTR_OutputNoisy));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.diffuse = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
			},
			[=](RayTracedReflectionsPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.normal),
					ctx.GetReadOnlyTexture(data.diffuse),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct RayTracedReflectionsConstants
				{
					Float   roughness_scale;
					Uint32  depth_idx;
					Uint32  normal_idx;
					Uint32  albedo_idx;
					Uint32  output_idx;
				} constants =
				{
					.roughness_scale = reflection_roughness_scale,
					.depth_idx = i + 0, .normal_idx = i + 1, .albedo_idx = i + 2, .output_idx = i + 3
				};
				GfxRayTracingShaderBindings* bindings = cmd_list->BeginRayTracingShaderBindings(ray_traced_reflections_pso.get());
				bindings->SetRayGenShader("RTR_RayGen");
				bindings->AddMissShader("RTR_Miss");
				bindings->AddHitGroup("RTRClosestHitGroupPrimaryRay");
				bindings->Commit();

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);
			}, RGPassType::Compute, RGPassFlags::None);
		
		blur_pass.AddPass(rg, RG_NAME(RTR_OutputNoisy), RG_NAME(RTR_Output), "RTR Denoise");
		copy_to_texture_pass.AddPass(rg, postprocessor->GetFinalResource(), RG_NAME(RTR_Output), BlendMode::AdditiveBlend);
	}

	void RayTracedReflectionsPass::OnResize(Uint32 w, Uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
		copy_to_texture_pass.OnResize(w, h);
	}

	Bool RayTracedReflectionsPass::IsEnabled(PostProcessor const*) const
	{
		return RTR.Get();
	}

	void RayTracedReflectionsPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Ray Traced Reflection", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Enable RTR", RTR.GetPtr());
					if (RTR.Get())
					{
						ImGui::SliderFloat("Roughness scale", &reflection_roughness_scale, 0.0f, 0.25f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Reflection);
	}

	Bool RayTracedReflectionsPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedReflectionsPass::CreateStateObject()
	{
		GfxShader const& rtr_shader = SM_GetGfxShader(LIB_Reflections);

		GfxRayTracingPipelineDesc rtr_pipeline_desc{};
		rtr_pipeline_desc.max_payload_size = sizeof(Float) * 4; 
		rtr_pipeline_desc.max_attribute_size = 8;
		rtr_pipeline_desc.max_recursion_depth = 2;
		rtr_pipeline_desc.global_root_signature = GfxRootSignatureID::Common;

		GfxRayTracingShaderLibrary rtr_library(&rtr_shader);
		rtr_pipeline_desc.libraries.push_back(rtr_library);

		GfxRayTracingHitGroup rtr_primary_hit_group = GfxRayTracingHitGroup::Triangle(
			"RTRClosestHitGroupPrimaryRay",    
			"RTR_ClosestHitPrimaryRay",        
			""                                 
		);
		rtr_pipeline_desc.hit_groups.push_back(rtr_primary_hit_group);

		/*
		GfxRayTracingHitGroup rtr_reflection_hit_group = GfxRayTracingHitGroup::Triangle(
			"RTRClosestHitGroupReflectionRay",
			"RTR_ClosestHitReflectionRay",
			""
		);
		rtr_pipeline_desc.hit_groups.push_back(rtr_reflection_hit_group);
		*/

		ray_traced_reflections_pso = gfx->CreateRayTracingPipeline(rtr_pipeline_desc);
		ADRIA_ASSERT(ray_traced_reflections_pso != nullptr);
		ADRIA_ASSERT(ray_traced_reflections_pso->IsValid());
	}

	void RayTracedReflectionsPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_Reflections)
		{
			CreateStateObject();
		}
	}

}


