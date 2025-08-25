#include "PathTracingPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "SVGFDenoiserPass.h"
#include "OIDNDenoiserPass.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineState.h"
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
	static TAutoConsoleVariable<Int> DenoiserThreshold("r.PathTracing.Denoiser.AccumulationThreshold", 64, "After how many accumulation frames we stop using denoiser");

	PathTracingPass::PathTracingPass(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), width(width), height(height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (is_supported)
		{
			CreateStateObjects();
			CreatePSO();
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
			accumulated_frames = 0;
		}

		denoiser_active = Denoiser.Get() != DenoiserType_None && (DenoiserThreshold.Get() == 0 || accumulated_frames < DenoiserThreshold.Get());

		AddPathTracingPass(rg);
		
		if (denoiser_active)
		{
			svgf_denoiser_pass->AddPass(rg);
			AddRemodulatePass(rg);
		}
		++accumulated_frames;
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
	}

	void PathTracingPass::GUI()
	{
		QueueGUI([&]()
			{
				ImGui::SliderInt("Max Bounces", MaxBounces.GetPtr(), 1, 8);
				ImGui::Checkbox("Accumulate Radiance", AccumulateRadiance.GetPtr());
				if (ImGui::Combo("Denoiser Type", Denoiser.GetPtr(), "None\0SVGF\0", 2))
				{
				}
				ImGui::SliderInt("Denoiser Threshold", DenoiserThreshold.GetPtr(), 0, 128);
				ImGui::Separator();
			}, GUICommandGroup_PathTracer
		);

		if (Denoiser.Get() != DenoiserType_None)
		{
			svgf_denoiser_pass->GUI();
		}
	}

	RGResourceName PathTracingPass::GetFinalOutput() const
	{
		return denoiser_active ? RG_NAME(PT_Denoised) : RG_NAME(PT_Output);
	}

	void PathTracingPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Remodulate;
		remodulate_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void PathTracingPass::CreateStateObjects()
	{
		GfxShaderKey pt_shader_key(LIB_PathTracing);
		GfxShader const& pt_blob = SM_GetGfxShader(pt_shader_key);
		path_tracing_so.reset(CreateStateObjectCommon(pt_shader_key));

		pt_shader_key.AddDefine("WITH_DENOISER", "1");
		GfxShader const& pt_blob_write_gbuffer = SM_GetGfxShader(pt_shader_key);
		path_tracing_with_denoiser_so.reset(CreateStateObjectCommon(pt_shader_key));
	}

	GfxStateObject* PathTracingPass::CreateStateObjectCommon(GfxShaderKey const& shader_key)
	{
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
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
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

	void PathTracingPass::AddPathTracingPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct PathTracingPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadWriteId accumulation;
			RGTextureReadWriteId albedo;
			RGTextureReadWriteId normal;
			RGTextureReadWriteId motion;
			RGTextureReadWriteId depth;
			RGTextureReadWriteId mesh_id;
			RGTextureReadWriteId specular;
		};

		rg.ImportTexture(RG_NAME(AccumulationTexture), accumulation_texture.get());

		rg.AddPass<PathTracingPassData>("Path Tracing Pass",
			[=](PathTracingPassData& data, RGBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(PT_Output), render_target_desc);

				data.output = builder.WriteTexture(RG_NAME(PT_Output));
				data.accumulation = builder.WriteTexture(RG_NAME(AccumulationTexture));
				if (denoiser_active)
				{
					RGTextureDesc denoiser_texture_desc{};
					denoiser_texture_desc.width = width;
					denoiser_texture_desc.height = height;
					denoiser_texture_desc.format = GfxFormat::R16G16B16A16_FLOAT;

					builder.DeclareTexture(RG_NAME(PT_Albedo), denoiser_texture_desc);
					builder.DeclareTexture(RG_NAME(PT_Normal), denoiser_texture_desc);
					builder.DeclareTexture(RG_NAME(PT_Specular), denoiser_texture_desc);
					denoiser_texture_desc.format = GfxFormat::R16G16_FLOAT;
					builder.DeclareTexture(RG_NAME(PT_MotionVectors), denoiser_texture_desc);
					denoiser_texture_desc.format = GfxFormat::R32_FLOAT;
					builder.DeclareTexture(RG_NAME(PT_Depth), denoiser_texture_desc);
					denoiser_texture_desc.format = GfxFormat::R32_UINT;
					builder.DeclareTexture(RG_NAME(PT_MeshID), denoiser_texture_desc);

					data.albedo = builder.WriteTexture(RG_NAME(PT_Albedo));
					data.normal = builder.WriteTexture(RG_NAME(PT_Normal));
					data.specular = builder.WriteTexture(RG_NAME(PT_Specular));
					data.motion = builder.WriteTexture(RG_NAME(PT_MotionVectors));
					data.depth = builder.WriteTexture(RG_NAME(PT_Depth));
					data.mesh_id = builder.WriteTexture(RG_NAME(PT_MeshID));
				}
			},
			[=](PathTracingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor const null_uav = gfxcommon::GetCommonView(GfxCommonViewType::NullTexture2D_UAV);
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadWriteTexture(data.accumulation),
					ctx.GetReadWriteTexture(data.output),
					denoiser_active ? ctx.GetReadWriteTexture(data.albedo) : null_uav,
					denoiser_active ? ctx.GetReadWriteTexture(data.normal) : null_uav,
					denoiser_active ? ctx.GetReadWriteTexture(data.motion) : null_uav,
					denoiser_active ? ctx.GetReadWriteTexture(data.depth) : null_uav,
					denoiser_active ? ctx.GetReadWriteTexture(data.mesh_id) : null_uav,
					denoiser_active ? ctx.GetReadWriteTexture(data.specular) : null_uav
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
					Uint32  albedo_idx;
					Uint32  normal_idx;
					Uint32  motion_vectors_idx;
					Uint32  depth_idx;
					Uint32  mesh_id_idx;
					Uint32  specular_idx;
				} constants =
				{
					.bounce_count = MaxBounces.Get(), .accumulated_frames = accumulated_frames,
					.accum_idx = i + 0, .output_idx = i + 1, .albedo_idx = i + 2, .normal_idx = i + 3,
					.motion_vectors_idx = i + 4,  .depth_idx = i + 5, .mesh_id_idx = i + 6, .specular_idx = i + 7
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				auto& table = cmd_list->SetStateObject(denoiser_active ? path_tracing_with_denoiser_so.get() : path_tracing_so.get());
				table.SetRayGenShader("PT_RayGen");
				cmd_list->DispatchRays(width, height);

			}, RGPassType::Compute, RGPassFlags::None);
	}

	void PathTracingPass::AddRemodulatePass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct RemodulatePassData
		{
			RGTextureReadOnlyId denoised_lighting;
			RGTextureReadOnlyId albedo;
			RGTextureReadOnlyId specular;
			RGTextureReadWriteId output;
		};

		rg.AddPass<RemodulatePassData>("Remodulation Pass",
			[=](RemodulatePassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(PT_Denoised), desc);

				data.denoised_lighting = builder.ReadTexture(svgf_denoiser_pass->GetOutputName(), ReadAccess_NonPixelShader);
				data.albedo = builder.ReadTexture(RG_NAME(PT_Albedo), ReadAccess_NonPixelShader);
				data.specular = builder.ReadTexture(RG_NAME(PT_Specular), ReadAccess_NonPixelShader); 
				data.output = builder.WriteTexture(RG_NAME(PT_Denoised));
			},
			[=](RemodulatePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.denoised_lighting),
					ctx.GetReadOnlyTexture(data.albedo),
					ctx.GetReadOnlyTexture(data.specular),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct RemodulatePassConstants
				{
					Uint32 denoised_lighting_idx;
					Uint32 albedo_idx;
					Uint32 specular_idx;
					Uint32 output_idx;
				} constants = {
					.denoised_lighting_idx = i,
					.albedo_idx = i + 1,
					.specular_idx = i + 2, 
					.output_idx = i + 3 
				};

				cmd_list->SetPipelineState(remodulate_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);

			}, RGPassType::Compute);
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





