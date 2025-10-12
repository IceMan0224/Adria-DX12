#include "PathTracingPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Components.h"
#include "SVGFDenoiserPass.h"
#include "OIDNDenoiserPass.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/D3D12/D3D12Device.h"
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
		GfxReflection::FillInputLayoutDesc(SM_GetGfxShader(VS_PT_GBuffer), pt_gbuffer_pso_desc.input_layout);
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
		path_tracing_so.reset(CreateStateObjectCommon(pt_shader_key));

		pt_shader_key.AddDefine("SVGF_ENABLED", "1");
		GfxShader const& pt_blob_write_gbuffer = SM_GetGfxShader(pt_shader_key);
		path_tracing_svgf_enabled_so.reset(CreateStateObjectCommon(pt_shader_key));
	}

	GfxStateObject* PathTracingPass::CreateStateObjectCommon(GfxShaderKey const& shader_key)
	{
		D3D12Device* d3d12_gfx = (D3D12Device*)gfx;
		GfxShader const& pt_blob = SM_GetGfxShader(shader_key);
		GfxStateObjectBuilder pt_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = pt_blob.GetSize();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = pt_blob.GetData();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			pt_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG pt_shader_config{};
			pt_shader_config.MaxPayloadSizeInBytes = sizeof(Float);
			pt_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			pt_state_object_builder.AddSubObject(pt_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = d3d12_gfx->GetCommonRootSignature();
			pt_state_object_builder.AddSubObject(global_root_sig);

			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 3;
			pt_state_object_builder.AddSubObject(pipeline_config);
		}
		return pt_state_object_builder.CreateStateObject(gfx);
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
			[=](RenderGraphBuilder& builder)
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
			[=](RenderGraphContext& context)
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
			[=](PathTracingPassData& data, RGBuilder& builder)
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
			[=](PathTracingPassData const& data, RenderGraphContext& ctx)
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
					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
					gfx->CopyDescriptors(dst_descriptor, src_descriptors);
					Uint32 const i = dst_descriptor.GetIndex();

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
						.direct_radiance_idx = i + 0, .indirect_radiance_idx = i + 1,
						.direct_albedo_idx = i + 2, .indirect_albedo_idx = i + 3
					};

					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					auto& table = cmd_list->SetStateObject(path_tracing_svgf_enabled_so.get());
					table.SetRayGenShader("PT_RayGen");
					cmd_list->DispatchRays(width, height);
				}
				else
				{
					GfxDescriptor src_descriptors[] =
					{
						ctx.GetReadWriteTexture(data.accumulation),
						ctx.GetReadWriteTexture(data.output)
					};
					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
					gfx->CopyDescriptors(dst_descriptor, src_descriptors);
					Uint32 const i = dst_descriptor.GetIndex();

					struct PathTracingConstants
					{
						Int32   bounce_count;
						Int32   accumulated_frames;
						Uint32  accum_idx;
						Uint32  output_idx;
					} constants =
					{
						.bounce_count = MaxBounces.Get(), .accumulated_frames = accumulated_frames,
						.accum_idx = i + 0, .output_idx = i + 1
					};

					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					auto& table = cmd_list->SetStateObject(path_tracing_so.get());
					table.SetRayGenShader("PT_RayGen");
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





