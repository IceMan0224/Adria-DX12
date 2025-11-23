#include "DDGIPass.h"
#include "BlackboardData.h"
#include "Components.h"
#include "ShaderStructs.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxRayTracingPipeline.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxRayTracingShaderBindings.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Constants.h"
#include "Editor/GUICommand.h"
#include "Utilities/Random.h"
#include "Core/ConsoleManager.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	static TAutoConsoleVariable<Bool> DDGI("r.DDGI", true, "Enable DDGI if supported");

	Vector2u DDGIPass::ProbeTextureDimensions(Vector3u const& num_probes, Uint32 texels_per_probe)
	{
		Uint32 width = (1 + texels_per_probe + 1) * num_probes.y * num_probes.x;
		Uint32 height = (1 + texels_per_probe + 1) * num_probes.z;
		return Vector2u(width, height);
	}

	DDGIPass::DDGIPass(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h) : gfx(gfx), reg(reg), width(w), height(h)
	{
		is_supported = gfx->GetCapabilities().SupportsRayTracing();
#if defined(ADRIA_PLATFORM_MACOS)
		DDGI->Set(false);
#else
		DDGI->Set(is_supported);
#endif
		if (is_supported)
		{
			CreatePSOs();
			CreateStateObject();
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&DDGIPass::OnLibraryRecompiled, *this);
		}
	}

	DDGIPass::~DDGIPass() = default;

	void DDGIPass::OnSceneInitialized()
	{
		BoundingBox scene_bounding_box;
		for (entt::entity mesh_entity : reg.view<Mesh>())
		{
			Mesh& mesh = reg.get<Mesh>(mesh_entity);

			for (SubMeshInstance const& instance : mesh.instances)
			{
				SubMeshGPU& submesh = mesh.submeshes[instance.submesh_index];
				BoundingBox instance_bounding_box;
				submesh.bounding_box.Transform(instance_bounding_box, instance.world_transform);
				BoundingBox::CreateMerged(scene_bounding_box, scene_bounding_box, instance_bounding_box);
			}
		}
		ddgi_volume.origin = scene_bounding_box.Center;
		ddgi_volume.extents = 1.1f * Vector3(scene_bounding_box.Extents);
		ddgi_volume.num_probes = Vector3u(16, 12, 14);
		ddgi_volume.num_rays = 128;
		ddgi_volume.max_num_rays = 512;

		Vector2u irradiance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_IRRADIANCE_TEXELS);
		GfxTextureDesc irradiance_desc{};
		irradiance_desc.width = irradiance_dimensions.x;
		irradiance_desc.height = irradiance_dimensions.y;
		irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		irradiance_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		irradiance_desc.initial_state = GfxResourceState::CopyDst;
		ddgi_volume.irradiance_history = gfx->CreateTexture(irradiance_desc);
		ddgi_volume.irradiance_history->SetName("DDGI Irradiance History");
		ddgi_volume.irradiance_history_srv = gfx->CreateTextureSRV(ddgi_volume.irradiance_history.get());

		Vector2u distance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_DISTANCE_TEXELS);
		GfxTextureDesc distance_desc{};
		distance_desc.width = distance_dimensions.x;
		distance_desc.height = distance_dimensions.y;
		distance_desc.format = GfxFormat::R16G16_FLOAT;
		distance_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		distance_desc.initial_state = GfxResourceState::CopyDst;
		ddgi_volume.distance_history = gfx->CreateTexture(distance_desc);
		ddgi_volume.distance_history->SetName("DDGI Distance History");
		ddgi_volume.distance_history_srv = gfx->CreateTextureSRV(ddgi_volume.distance_history.get());
	}

	void DDGIPass::OnResize(Uint32 w, Uint32 h)
	{
		if (!IsSupported())
		{
			return;
		}
		width = w, height = h;
	}

	void DDGIPass::AddPasses(RenderGraph& rg)
	{
		ADRIA_ASSERT(IsSupported());
		RG_SCOPE(rg, "DDGI");

		Uint32 const num_probes_flat = ddgi_volume.num_probes.x * ddgi_volume.num_probes.y * ddgi_volume.num_probes.z;
		RealRandomGenerator rng(0.0f, 1.0f);
		Vector3 random_vector(2.0f * rng() - 1.0f, 2.0f * rng() - 1.0f, 2.0f * rng() - 1.0f); 
		random_vector.Normalize();
		Float random_angle = rng() * pi<Float> * 2.0f;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.ImportTexture(RG_NAME(DDGIIrradianceHistory), ddgi_volume.irradiance_history.get());
		rg.ImportTexture(RG_NAME(DDGIDistanceHistory), ddgi_volume.distance_history.get());

		struct DDGIBlackboardData
		{
			Uint32 heap_index;
		};

		if (gfx->IsFirstFrame())
		{
			struct DDGIClearHistoryPassData
			{
				RGTextureReadWriteId irradiance_history;
				RGTextureReadWriteId distance_history;
			};

			rg.AddPass<DDGIClearHistoryPassData>("DDGI Clear History Pass",
				[=, this](DDGIClearHistoryPassData& data, RenderGraphBuilder& builder)
				{
					data.irradiance_history = builder.WriteTexture(RG_NAME(DDGIIrradianceHistory));
					data.distance_history = builder.WriteTexture(RG_NAME(DDGIDistanceHistory));
				},
				[=, this](DDGIClearHistoryPassData const& data, RenderGraphContext& ctx) mutable
				{
					GfxCommandList* cmd_list = ctx.GetCommandList();
					static constexpr Float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					cmd_list->ClearTexture(ctx.GetTexture(*data.irradiance_history), black);
					cmd_list->ClearTexture(ctx.GetTexture(*data.distance_history), black);
				}, RGPassType::Compute);
		}

		struct DDGIRayTracePassData
		{
			RGBufferReadWriteId ray_buffer;
			RGTextureReadOnlyId irradiance_history;
			RGTextureReadOnlyId distance_history;
		};

		rg.AddPass<DDGIRayTracePassData>("DDGI Ray Trace Pass",
			[=, this](DDGIRayTracePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc ray_buffer_desc{};
				ray_buffer_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				ray_buffer_desc.stride = GetGfxFormatStride(ray_buffer_desc.format);
				ray_buffer_desc.size = ray_buffer_desc.stride * num_probes_flat * ddgi_volume.max_num_rays;
				builder.DeclareBuffer(RG_NAME(DDGIRayBuffer), ray_buffer_desc);

				data.ray_buffer = builder.WriteBuffer(RG_NAME(DDGIRayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_NAME(DDGIIrradianceHistory));
				data.distance_history = builder.ReadTexture(RG_NAME(DDGIDistanceHistory));
			},
			[=, this](DDGIRayTracePassData const& data, RenderGraphContext& ctx) mutable
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptor[] = 
				{
					ctx.GetReadWriteBuffer(data.ray_buffer)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptor);
				ctx.GetBlackboard().Create<DDGIBlackboardData>(table.base);

				struct DDGIParameters
				{
					Vector3  random_vector;
					Float    random_angle;
					Float    history_blend_weight;
					Uint32   ray_buffer_index;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = table
				};

				GfxRayTracingShaderBindings* bindings = cmd_list->BeginRayTracingShaderBindings(ddgi_trace_pso.get());
				bindings->SetRayGenShader("DDGI_RayGen");
				bindings->AddMissShader("DDGI_Miss");
				bindings->AddHitGroup("DDGI_HitGroup");
				bindings->Commit();

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				
				cmd_list->DispatchRays(ddgi_volume.num_rays, num_probes_flat);
				cmd_list->BufferBarrier(ctx.GetBuffer(*data.ray_buffer), GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
			}, RGPassType::Compute);

		struct DDGIUpdateIrradiancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		irradiance_history;
			RGTextureReadWriteId	irradiance;
		};

		rg.AddPass<DDGIUpdateIrradiancePassData>("DDGI Update Irradiance Pass",
			[=, this](DDGIUpdateIrradiancePassData& data, RenderGraphBuilder& builder)
			{
				Vector2u irradiance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_IRRADIANCE_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width  = irradiance_dimensions.x;
				irradiance_desc.height = irradiance_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(DDGIIrradiance), irradiance_desc);

				data.irradiance		= builder.WriteTexture(RG_NAME(DDGIIrradiance));
				data.ray_buffer		= builder.ReadBuffer(RG_NAME(DDGIRayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_NAME(DDGIIrradianceHistory));
			},
			[=, this](DDGIUpdateIrradiancePassData const& data, RenderGraphContext& ctx) mutable
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				DDGIBlackboardData const& ddgi_blackboard = ctx.GetBlackboard().Get<DDGIBlackboardData>();

				GfxDescriptor src_descriptor[] = 
				{
					ctx.GetReadWriteTexture(data.irradiance)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptor);

				struct DDGIParameters
				{
					Vector3  random_vector;
					Float    random_angle;
					Float    history_blend_weight;
					Uint32   ray_buffer_index;
					Uint32   irradiance_idx;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = ddgi_blackboard.heap_index,
					.irradiance_idx = table
				};

				cmd_list->SetPipelineState(update_irradiance_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes_flat, 1, 1);
				cmd_list->TextureBarrier(ctx.GetTexture(*data.irradiance), GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
			}, RGPassType::Compute);

		struct DDGIUpdateDistancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		distance_history;
			RGTextureReadWriteId	distance;
		};

		rg.AddPass<DDGIUpdateDistancePassData>("DDGI Update Distance Pass",
			[=, this](DDGIUpdateDistancePassData& data, RenderGraphBuilder& builder)
			{
				Vector2u distance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_DISTANCE_TEXELS);
				RGTextureDesc distance_desc{};
				distance_desc.width = distance_dimensions.x;
				distance_desc.height = distance_dimensions.y;
				distance_desc.format = GfxFormat::R16G16_FLOAT;
				builder.DeclareTexture(RG_NAME(DDGIDistance), distance_desc);

				data.distance = builder.WriteTexture(RG_NAME(DDGIDistance));
				data.ray_buffer = builder.ReadBuffer(RG_NAME(DDGIRayBuffer));
				data.distance_history = builder.ReadTexture(RG_NAME(DDGIDistanceHistory));
			},
			[=, this](DDGIUpdateDistancePassData const& data, RenderGraphContext& ctx) mutable
			{
				GfxDevice* gfx = ctx.GetDevice();
				GfxCommandList* cmd_list = ctx.GetCommandList();

				DDGIBlackboardData const& ddgi_blackboard = ctx.GetBlackboard().Get<DDGIBlackboardData>();

				GfxDescriptor src_descriptor[] =
				{
					ctx.GetReadWriteTexture(data.distance)
				};
				GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptor);

				struct DDGIParameters
				{
					Vector3  random_vector;
					Float    random_angle;
					Float    history_blend_weight;
					Uint32   ray_buffer_index;
					Uint32   distance_idx;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = ddgi_blackboard.heap_index,
					.distance_idx = table
				};

				cmd_list->SetPipelineState(update_distance_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes_flat, 1, 1);
				cmd_list->TextureBarrier(ctx.GetTexture(*data.distance), GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
			}, RGPassType::Compute);

		rg.ExportTexture(RG_NAME(DDGIIrradiance), ddgi_volume.irradiance_history.get());
		rg.ExportTexture(RG_NAME(DDGIDistance), ddgi_volume.distance_history.get());
	}

	void DDGIPass::AddVisualizePass(RenderGraph& rg)
	{
		if (!IsSupported() || !visualize)
		{
			return;
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<void>("DDGI Visualize Pass",
			[=, this](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=, this](RenderGraphContext& ctx) mutable
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				struct DDGIVisualizeParameters
				{
					Uint32 visualize_mode;
				} parameters
				{
					.visualize_mode = (Uint32)ddgi_visualize_mode
				};
				cmd_list->SetPrimitiveTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->SetPipelineState(visualize_probes_pso->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Draw(2880, ddgi_volume.num_probes.x * ddgi_volume.num_probes.y * ddgi_volume.num_probes.z);
			}, RGPassType::Graphics);
	}

	void DDGIPass::GUI()
	{
		if (!is_supported)
		{
			return;
		}

		QueueGUI([&]()
			{
				if (ImGui::TreeNode("DDGI"))
				{
					ImGui::Checkbox("Enable", DDGI.GetPtr());
					if (DDGI.Get())
					{
						ImGui::Checkbox("Visualize DDGI", &visualize);
						if (visualize)
						{
							static const Char* visualize_mode[] = { "Irradiance", "Distance" };
							static Int current_visualize_mode = 0;
							const Char* visualize_mode_label = visualize_mode[current_visualize_mode];
							if (ImGui::BeginCombo("DDGI Visualize Mode", visualize_mode_label, 0))
							{
								for (Int n = 0; n < IM_ARRAYSIZE(visualize_mode); n++)
								{
									const Bool is_selected = (current_visualize_mode == n);
									if (ImGui::Selectable(visualize_mode[n], is_selected)) current_visualize_mode = n;
									if (is_selected) ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
							ddgi_visualize_mode = (DDGIVisualizeMode)current_visualize_mode;
						}
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer);
	}

	Bool DDGIPass::IsEnabled() const
	{
		return DDGI.Get();
	}

	Int32 DDGIPass::GetDDGIVolumeIndex()
	{
		if (!IsSupported())
		{
			return -1;
		}

		std::vector<DDGIVolumeGPU> ddgi_data;
		DDGIVolumeGPU& ddgi_gpu = ddgi_data.emplace_back();
		ddgi_gpu.start_position = ddgi_volume.origin - ddgi_volume.extents;
		ddgi_gpu.probe_size = 2 * ddgi_volume.extents / (Vector3((Float)ddgi_volume.num_probes.x, (Float)ddgi_volume.num_probes.y, (Float)ddgi_volume.num_probes.z) - Vector3::One);
		ddgi_gpu.rays_per_probe = ddgi_volume.num_rays;
		ddgi_gpu.max_rays_per_probe = ddgi_volume.max_num_rays;
		ddgi_gpu.probe_count = Vector3i(ddgi_volume.num_probes.x, ddgi_volume.num_probes.y, ddgi_volume.num_probes.z);
		ddgi_gpu.normal_bias = 0.25f;
		ddgi_gpu.energy_preservation = 0.85f;

		if (!ddgi_volume_buffer || ddgi_volume_buffer->GetCount() < ddgi_data.size())
		{
			ddgi_volume_buffer = gfx->CreateBuffer(StructuredBufferDesc<DDGIVolumeGPU>(ddgi_data.size(), false, true));
			ddgi_volume_buffer_srv = gfx->CreateBufferSRV(ddgi_volume_buffer.get());
		}

		GfxDescriptor src_descriptors[] =
		{
			ddgi_volume.irradiance_history_srv,
			ddgi_volume.distance_history_srv,
			ddgi_volume_buffer_srv
		};
		GfxBindlessTable table = gfx->AllocateAndUpdateBindlessTable(src_descriptors);
		Int32 table_base_index = static_cast<Int32>(table);

		ddgi_gpu.irradiance_history_idx = table_base_index;
		ddgi_gpu.distance_history_idx = table_base_index + 1;
		ddgi_volume_buffer->Update(ddgi_data.data(), ddgi_data.size() * sizeof(DDGIVolumeGPU));

		return table_base_index + 2;
	}

	void DDGIPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc  gfx_pso_desc{};
		gfx_pso_desc.input_layout.elements.clear();
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_DDGIVisualize;
		gfx_pso_desc.PS = PS_DDGIVisualize;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.num_render_targets = 1u;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Triangle;
		visualize_probes_pso = gfx->CreateManagedGraphicsPipelineState(gfx_pso_desc);

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_DDGIUpdateIrradiance;
		update_irradiance_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_DDGIUpdateDistance;
		update_distance_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

	void DDGIPass::CreateStateObject()
	{
		GfxShader const& ddgi_shader = SM_GetGfxShader(LIB_DDGIRayTracing);
		GfxRayTracingPipelineDesc ddgi_pipeline_desc = {};
		ddgi_pipeline_desc.max_payload_size = 16;
		ddgi_pipeline_desc.max_attribute_size = 8;
		ddgi_pipeline_desc.max_recursion_depth = 1;
		ddgi_pipeline_desc.global_root_signature = GfxRootSignatureID::Common;

		GfxRayTracingShaderLibrary ddgi_library(&ddgi_shader,
		{
			"DDGI_RayGen",
			"DDGI_Miss",
			"DDGI_ClosestHit"
		});
		ddgi_pipeline_desc.libraries.push_back(ddgi_library);

		GfxRayTracingHitGroup ddgi_hit_group = GfxRayTracingHitGroup::Triangle(
			"DDGI_HitGroup",
			"DDGI_ClosestHit",
			""
		);
		ddgi_pipeline_desc.hit_groups.push_back(ddgi_hit_group);
		ddgi_trace_pso = gfx->CreateRayTracingPipeline(ddgi_pipeline_desc);
		ADRIA_ASSERT(ddgi_trace_pso != nullptr);
		ADRIA_ASSERT(ddgi_trace_pso->IsValid());
	}

	void DDGIPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_DDGIRayTracing)
		{
			CreateStateObject();
		}
	}

}

