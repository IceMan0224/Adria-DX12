#include "PathTracingPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Components.h"
#include "SVGFDenoiserPass.h"
#include "OIDNDenoiserPass.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxRayTracingPipeline.h"
#include "Graphics/GfxRayTracingShaderBindings.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxBufferView.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	enum DenoiserType : Uint8
	{
		DenoiserType_None,
		DenoiserType_SVGF
	};
	static TAutoConsoleVariable<Bool> AccumulateRadiance("r.PathTracing.AccumulateRadiance", true, "Should we accumulate radiance in path tracer or no");
	static TAutoConsoleVariable<Int> MaxBounces("r.PathTracing.MaxBounces", 3, "Maximum number of bounces in a path tracer");
	static TAutoConsoleVariable<Int> Denoiser("r.PathTracing.Denoiser", DenoiserType_None, "What denoiser will path tracer use: 0 - None, 1 - SVGF");

	PathTracingPass::PathTracingPass(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height)
		: reg(reg), gfx(gfx), width(width), height(height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (is_supported)
		{
			CreateStateObjects();
			CreatePSOs();
			svgf_denoiser_pass = std::make_unique<SVGFDenoiserPass>(gfx, width, height);
			OnResize(width, height);
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&PathTracingPass::OnLibraryRecompiled, *this);
		}
	}
	PathTracingPass::~PathTracingPass() = default;

	void PathTracingPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported())
		{
			return;
		}

		if (!AccumulateRadiance.Get())
		{
			accumulated_frames = 1;
		}

		denoiser_active = Denoiser.Get() != DenoiserType_None;
		if (denoiser_active)
		{
			AddPTGBufferPass(rg);
			AddPathTracingPass(rg);
			svgf_denoiser_pass->AddPass(rg);
		}
		else
		{
			AddPathTracingPass(rg);
			if (AccumulateRadiance.Get())
			{
				++accumulated_frames;
			}
		}

	}

	void PathTracingPass::OnResize(Uint32 w, Uint32 h)
	{
		if (!IsSupported())
		{
			return;
		}

		width = w, height = h;
		svgf_denoiser_pass->OnResize(w, h);
		CreateAccumulationTexture();
	}

	Bool PathTracingPass::IsSupported() const
	{
		return is_supported;
	}

	void PathTracingPass::Reset()
	{
		accumulated_frames = 0;
		svgf_denoiser_pass->Reset();
	}

	void PathTracingPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Path Tracing Settings", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderInt("Max Bounces", MaxBounces.GetPtr(), 1, 8);
					if (!denoiser_active)
					{
						ImGui::Checkbox("Accumulate Radiance", AccumulateRadiance.GetPtr());
					}
					ImGui::Combo("Denoiser Type", Denoiser.GetPtr(), "None\0SVGF\0", 2);

					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer);

		if (denoiser_active)
		{
			svgf_denoiser_pass->GUI();
		}
	}

	RGResourceName PathTracingPass::GetFinalOutput() const
	{
		return denoiser_active ? RG_NAME(PT_Denoised) : RG_NAME(PT_Output);
	}

	void PathTracingPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc pt_gbuffer_pso_desc{};
		GfxShaderCompiler::FillInputLayoutDesc(SM_GetGfxShader(VS_PT_GBuffer), pt_gbuffer_pso_desc.input_layout);
		pt_gbuffer_pso_desc.root_signature = GfxRootSignatureID::Common;
		pt_gbuffer_pso_desc.VS = VS_PT_GBuffer;
		pt_gbuffer_pso_desc.PS = PS_PT_GBuffer;
		pt_gbuffer_pso_desc.depth_state.depth_enable = true;
		pt_gbuffer_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		pt_gbuffer_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		pt_gbuffer_pso_desc.num_render_targets = 3u;
		pt_gbuffer_pso_desc.rtv_formats[0] = GfxFormat::R32G32B32A32_FLOAT;
		pt_gbuffer_pso_desc.rtv_formats[1] = GfxFormat::R16G16B16A16_FLOAT;
		pt_gbuffer_pso_desc.rtv_formats[2] = GfxFormat::R32G32B32A32_FLOAT;
		pt_gbuffer_pso_desc.dsv_format = GfxFormat::D32_FLOAT;

		pt_gbuffer_pso = std::make_unique<GfxGraphicsPipelineState>(gfx, pt_gbuffer_pso_desc);
	}

	void PathTracingPass::CreateStateObjects()
	{
		GfxShaderKey pt_shader_key(LIB_PathTracing);
		GfxShader const& pt_blob = SM_GetGfxShader(pt_shader_key);
		path_tracing_pso = CreateRayTracingPipelineCommon(pt_shader_key);

		pt_shader_key.AddDefine("SVGF_ENABLED", "1");
		GfxShader const& pt_blob_write_gbuffer = SM_GetGfxShader(pt_shader_key);
		path_tracing_svgf_enabled_pso = CreateRayTracingPipelineCommon(pt_shader_key);
	}

	std::unique_ptr<GfxRayTracingPipeline> PathTracingPass::CreateRayTracingPipelineCommon(GfxShaderKey const& shader_key)
	{
		GfxShader const& pt_shader = SM_GetGfxShader(shader_key);

		GfxRayTracingPipelineDesc pt_pipeline_desc = {};
		pt_pipeline_desc.max_payload_size = sizeof(Float);  
		pt_pipeline_desc.max_attribute_size = 8;
		pt_pipeline_desc.max_recursion_depth = 3;
		pt_pipeline_desc.global_root_signature = GfxRootSignatureID::Common;

		GfxRayTracingShaderLibrary pt_library(&pt_shader);
		pt_pipeline_desc.libraries.push_back(pt_library);

		std::unique_ptr<GfxRayTracingPipeline> pipeline = gfx->CreateRayTracingPipeline(pt_pipeline_desc);
		ADRIA_ASSERT(pipeline != nullptr);
		ADRIA_ASSERT(pipeline->IsValid());
		return pipeline;
	}

	void PathTracingPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_PathTracing)
		{
			CreateStateObjects();
		}
	}

	void PathTracingPass::AddPTGBufferPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<void>("Path Tracing GBuffer Pass",
			[=, this](RenderGraphBuilder& builder)
			{
				RGTextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				gbuffer_desc.format = GfxFormat::R32G32B32A32_FLOAT;
				builder.DeclareTexture(RG_NAME(PT_GBuffer_LinearZ), gbuffer_desc);

				gbuffer_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(PT_GBuffer_MotionVectors), gbuffer_desc);

				gbuffer_desc.format = GfxFormat::R32G32B32A32_FLOAT;
				builder.DeclareTexture(RG_NAME(PT_GBuffer_CompactNormDepth), gbuffer_desc);

				builder.WriteRenderTarget(RG_NAME(PT_GBuffer_LinearZ), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_NAME(PT_GBuffer_MotionVectors), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_NAME(PT_GBuffer_CompactNormDepth), RGLoadStoreAccessOp::Clear_Preserve);

				RGTextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = GfxFormat::R32_TYPELESS;
				depth_desc.clear_value = GfxClearValue(0.0f, 0);
				builder.DeclareTexture(RG_NAME(PT_DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_NAME(PT_DepthStencil), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);
			},
			[=, this](RenderGraphContext& context)
			{
				GfxDevice* gfx = context.GetDevice();
				GfxCommandList* cmd_list = context.GetCommandList();

				reg.sort<Batch>([&frame_data](Batch const& lhs, Batch const& rhs)
					{
						if (lhs.alpha_mode != rhs.alpha_mode)
						{
							return lhs.alpha_mode < rhs.alpha_mode;
						}
						if (lhs.shading_extension != rhs.shading_extension)
						{
							return lhs.shading_extension < rhs.shading_extension;
						}
						Vector3 camera_position(frame_data.camera_position);
						Float lhs_distance = Vector3::DistanceSquared(camera_position, lhs.bounding_box.Center);
						Float rhs_distance = Vector3::DistanceSquared(camera_position, rhs.bounding_box.Center);
						return lhs_distance < rhs_distance;
					});

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				auto view = reg.view<Batch>();
				for (entt::entity batch_entity : view)
				{
					Batch& batch = view.get<Batch>(batch_entity);
					if (!batch.camera_visibility)
					{
						continue;
					}

					GfxPipelineState const* pso = pt_gbuffer_pso->Get();
					cmd_list->SetPipelineState(pso);

					struct GBufferConstants
					{
						Uint32 instance_id;
					} constants{ .instance_id = batch.instance_id };
					cmd_list->SetRootConstants(1, constants);

					GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
					cmd_list->SetPrimitiveTopology(batch.submesh->topology);
					cmd_list->SetIndexBuffer(&ibv);
					cmd_list->DrawIndexed(batch.submesh->indices_count);
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void PathTracingPass::AddPathTracingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct PathTracingPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadWriteId accumulation;

			RGTextureReadWriteId direct_radiance;
			RGTextureReadWriteId indirect_radiance;
			RGTextureReadWriteId direct_albedo;
			RGTextureReadWriteId indirect_albedo;
		};

		rg.ImportTexture(RG_NAME(PT_AccumulationTexture), accumulation_texture.get());

		rg.AddPass<PathTracingPassData>("Path Tracing Pass",
			[=, this](PathTracingPassData& data, RGBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				if (denoiser_active)
				{
					RGTextureDesc denoiser_texture_desc{};
					denoiser_texture_desc.format = GfxFormat::R16G16B16A16_FLOAT;
					denoiser_texture_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
					denoiser_texture_desc.width = width;
					denoiser_texture_desc.height = height;
					builder.DeclareTexture(RG_NAME(PT_DirectRadiance), denoiser_texture_desc);
					builder.DeclareTexture(RG_NAME(PT_IndirectRadiance), denoiser_texture_desc);
					builder.DeclareTexture(RG_NAME(PT_DirectAlbedo), denoiser_texture_desc);
					builder.DeclareTexture(RG_NAME(PT_IndirectAlbedo), denoiser_texture_desc);

					data.direct_radiance = builder.WriteTexture(RG_NAME(PT_DirectRadiance));
					data.indirect_radiance = builder.WriteTexture(RG_NAME(PT_IndirectRadiance));
					data.direct_albedo = builder.WriteTexture(RG_NAME(PT_DirectAlbedo));
					data.indirect_albedo = builder.WriteTexture(RG_NAME(PT_IndirectAlbedo));
				}
				else
				{
					builder.DeclareTexture(RG_NAME(PT_Output), render_target_desc);
					data.accumulation = builder.WriteTexture(RG_NAME(PT_AccumulationTexture));
					data.output = builder.WriteTexture(RG_NAME(PT_Output));
				}
			},
			[=, this](PathTracingPassData const& data, RenderGraphContext& ctx)
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				if (denoiser_active)
				{
					GfxDescriptor src_descriptors[] =
					{
						ctx.GetReadWriteTexture(data.direct_radiance),
						ctx.GetReadWriteTexture(data.indirect_radiance),
						ctx.GetReadWriteTexture(data.direct_albedo),
						ctx.GetReadWriteTexture(data.indirect_albedo),
					};
					GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

					struct PathTracingConstants
					{
						Int32   bounce_count;
						Int32   accumulated_frames;
						Uint32  direct_radiance_idx;
						Uint32  indirect_radiance_idx;
						Uint32  direct_albedo_idx;
						Uint32  indirect_albedo_idx;
					} constants =
					{
						.bounce_count = MaxBounces.Get(), .accumulated_frames = accumulated_frames,
						.direct_radiance_idx = table + 0, .indirect_radiance_idx = table + 1,
						.direct_albedo_idx = table + 2, .indirect_albedo_idx = table + 3
					};

					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);

					GfxRayTracingShaderBindings* bindings = cmd_list->BeginRayTracingShaderBindings(path_tracing_svgf_enabled_pso.get());
					bindings->SetRayGenShader("PT_RayGen");
					bindings->Commit();
					cmd_list->DispatchRays(width, height);
				}
				else
				{
					GfxDescriptor src_descriptors[] =
					{
						ctx.GetReadWriteTexture(data.accumulation),
						ctx.GetReadWriteTexture(data.output)
					};
					GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);

					struct PathTracingConstants
					{
						Int32   bounce_count;
						Int32   accumulated_frames;
						Uint32  accum_idx;
						Uint32  output_idx;
					} constants =
					{
						.bounce_count = MaxBounces.Get(), .accumulated_frames = accumulated_frames,
						.accum_idx = table + 0, .output_idx = table + 1
					};

					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					GfxRayTracingShaderBindings* bindings = cmd_list->BeginRayTracingShaderBindings(path_tracing_pso.get());
					bindings->SetRayGenShader("PT_RayGen");
					bindings->Commit();
					cmd_list->DispatchRays(width, height);
				}

			}, RGPassType::Compute, RGPassFlags::None);
	}

	void PathTracingPass::CreateAccumulationTexture()
	{
		if (!accumulation_texture || accumulation_texture->GetWidth() != width || accumulation_texture->GetHeight() != height)
		{
			GfxTextureDesc accum_desc{};
			accum_desc.width = width;
			accum_desc.height = height;
			accum_desc.format = GfxFormat::R32G32B32A32_FLOAT;
			accum_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			accum_desc.initial_state = GfxResourceState::ComputeUAV;
			accumulation_texture = gfx->CreateTexture(accum_desc);
		}
	}
}





