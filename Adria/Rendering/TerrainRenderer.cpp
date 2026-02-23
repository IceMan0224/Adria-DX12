#include "nfd.h"
#include "TerrainRenderer.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "Utilities/Image.h"
#include "Core/Paths.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "Graphics/GfxCommon.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	TerrainRenderer::TerrainRenderer(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h)
		: reg{ reg }, gfx{ gfx }, width{ w }, height{ h }
	{
		CreatePSOs();
		CreatePatchMesh();
	}

	TerrainRenderer::~TerrainRenderer() = default;

	void TerrainRenderer::AddPasses(RenderGraph& rendergraph)
	{
		auto terrain_view = reg.view<Terrain>();
		if (terrain_view.empty())
		{
			return;
		}
		entt::entity terrain_entity = *terrain_view.begin();
		Terrain const& terrain = terrain_view.get<Terrain>(terrain_entity);

		if (!gpu_resources_initialized)
		{
			InitializeGPUResources();
		}
		if (!heightmap_texture) 
		{
			return;
		}

		RG_SCOPE(rendergraph, "Terrain");
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		if (!normals_generated && heightmap_texture && normalmap_texture)
		{
			rendergraph.ImportTexture(RG_NAME(TerrainHeightmap), heightmap_texture.get());
			rendergraph.ImportTexture(RG_NAME(TerrainNormalmap), normalmap_texture.get());

			struct TerrainNormalsPassData
			{
				RGTextureReadOnlyId heightmap;
				RGTextureReadWriteId normalmap;
			};
			rendergraph.AddPass<TerrainNormalsPassData>("Terrain Normals Pass",
				[=, this](TerrainNormalsPassData& data, RenderGraphBuilder& builder)
				{
					data.heightmap = builder.ReadTexture(RG_NAME(TerrainHeightmap), ReadAccess_NonPixelShader);
					data.normalmap = builder.WriteTexture(RG_NAME(TerrainNormalmap));
				},
				[=, this, &terrain](TerrainNormalsPassData const& data, RenderGraphContext& ctx)
				{
					GfxCommandList* cmd_list = ctx.GetCommandList();

					struct TerrainNormalsConstants
					{
						Uint32 heightmap_idx;
						Uint32 output_idx;
						Float  texel_size;
						Float  height_scale;
					} constants =
					{
						.heightmap_idx = ctx.GetReadOnlyTextureIndex(data.heightmap),
						.output_idx = ctx.GetReadWriteTextureIndex(data.normalmap),
						.texel_size = terrain.terrain_width / (Float)heightmap_width,
						.height_scale = terrain.height_scale
					};

					cmd_list->SetPipelineState(terrain_normals_pso->Get());
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
					cmd_list->Dispatch((heightmap_width + 15) / 16, (heightmap_depth + 15) / 16, 1);
				}, RGPassType::Compute, RGPassFlags::ForceNoCull);

			normals_generated = true;
		}

		Float cam_x = frame_data.camera_position[0];
		Float cam_z = frame_data.camera_position[2];
		BuildQuadtree(cam_x, cam_z, terrain.terrain_width, terrain.terrain_depth);

		if (visible_patches.empty()) 
		{
			return;
		}

		TextureHandle resolved_albedo[4], resolved_normal[4], resolved_arm[4];
		for (Uint32 i = 0; i < 4; ++i)
		{
			resolved_albedo[i] = terrain.layer_albedo[i] != INVALID_TEXTURE_HANDLE ? terrain.layer_albedo[i] : DEFAULT_WHITE_TEXTURE_HANDLE;
			resolved_normal[i] = terrain.layer_normal[i] != INVALID_TEXTURE_HANDLE ? terrain.layer_normal[i] : DEFAULT_NORMAL_TEXTURE_HANDLE;
			resolved_arm[i]    = terrain.layer_arm[i]    != INVALID_TEXTURE_HANDLE ? terrain.layer_arm[i]    : DEFAULT_WHITE_TEXTURE_HANDLE;
		}

		rendergraph.AddPass<void>("Terrain GBuffer Pass",
			[=, this](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_NAME(GBufferNormal), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferEmissive), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_NAME(GBufferCustom), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=, this, &terrain](RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxGraphicsPipelineStatePermutations* active_psos = terrain_psos.get();
				if (wireframe) 
				{
					active_psos->SetFillMode(GfxFillMode::Wireframe);
				}
				cmd_list->SetPipelineState(active_psos->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				struct TerrainConstants
				{
					Float  terrain_width;
					Float  terrain_depth;
					Float  height_scale;
					Float  min_tess_distance;
					Float  max_tess_distance;
					Float  min_tess_factor;
					Float  max_tess_factor;
					Uint32 heightmap_idx;
					Uint32 normalmap_idx;
					Uint32 splatmap_idx;
					Uint32 layer_albedo_idx[4];
					Uint32 layer_normal_idx[4];
					Uint32 layer_arm_idx[4];
					Float  layer_tiling[4];
				} constants =
				{
					.terrain_width = terrain.terrain_width,
					.terrain_depth = terrain.terrain_depth,
					.height_scale = terrain.height_scale,
					.min_tess_distance = 50.0f,
					.max_tess_distance = 500.0f,
					.min_tess_factor = 1.0f,
					.max_tess_factor = 16.0f,
					.heightmap_idx = gfx->GetBindlessDescriptorIndex(heightmap_srv),
					.normalmap_idx = gfx->GetBindlessDescriptorIndex(normalmap_srv),
					.splatmap_idx = gfx->GetBindlessDescriptorIndex(splatmap_srv),
					.layer_albedo_idx =
					{
						g_TextureManager.GetBindlessIndex(resolved_albedo[0]),
						g_TextureManager.GetBindlessIndex(resolved_albedo[1]),
						g_TextureManager.GetBindlessIndex(resolved_albedo[2]),
						g_TextureManager.GetBindlessIndex(resolved_albedo[3])
					},
					.layer_normal_idx =
					{
						g_TextureManager.GetBindlessIndex(resolved_normal[0]),
						g_TextureManager.GetBindlessIndex(resolved_normal[1]),
						g_TextureManager.GetBindlessIndex(resolved_normal[2]),
						g_TextureManager.GetBindlessIndex(resolved_normal[3])
					},
					.layer_arm_idx =
					{
						g_TextureManager.GetBindlessIndex(resolved_arm[0]),
						g_TextureManager.GetBindlessIndex(resolved_arm[1]),
						g_TextureManager.GetBindlessIndex(resolved_arm[2]),
						g_TextureManager.GetBindlessIndex(resolved_arm[3])
					},
					.layer_tiling = { terrain.layer_tiling[0], terrain.layer_tiling[1], terrain.layer_tiling[2], terrain.layer_tiling[3] }
				};
				cmd_list->SetRootCBV(2, constants);
				GfxVertexBufferView vbv(patch_vertex_buffer.get());
				cmd_list->SetVertexBuffers({ &vbv, 1 });
				GfxIndexBufferView ibv(patch_index_buffer.get());
				cmd_list->SetIndexBuffer(&ibv);

				for (auto const& patch : visible_patches)
				{
					struct TerrainPatchConstants
					{
						Float patch_world_pos_x;
						Float patch_world_pos_z;
						Float patch_size;
						Uint32 pad0;
					} patch_constants =
					{
						.patch_world_pos_x = patch.x,
						.patch_world_pos_z = patch.z,
						.patch_size = patch.size,
						.pad0 = 0
					};
					cmd_list->SetRootCBV(3, patch_constants);
					cmd_list->SetPrimitiveTopology(GfxPrimitiveTopology::TriangleList);
					cmd_list->DrawIndexed(patch_index_count);
				}
			},
			RGPassType::Graphics, RGPassFlags::None);
	}

	void TerrainRenderer::GUI()
	{
		QueueGUI([&]()
			{
				auto terrain_view = reg.view<Terrain>();
				if (terrain_view.begin() == terrain_view.end()) return;

				entt::entity terrain_entity = *terrain_view.begin();
				Terrain& terrain = terrain_view.get<Terrain>(terrain_entity);

				if (ImGui::TreeNodeEx("Terrain Settings", 0))
				{
					ImGui::Checkbox("Wireframe", &wireframe);
					Bool needs_rebuild = false;
					needs_rebuild |= ImGui::SliderFloat("Height Scale", &terrain.height_scale, 0.0f, 500.0f);
					ImGui::SliderFloat("Terrain Width", &terrain.terrain_width, 64.0f, 4096.0f);
					ImGui::SliderFloat("Terrain Depth", &terrain.terrain_depth, 64.0f, 4096.0f);
					if (ImGui::TreeNodeEx("Layer Tiling", 0))
					{
						ImGui::SliderFloat("Layer 0##tiling", &terrain.layer_tiling[0], 1.0f, 256.0f);
						ImGui::SliderFloat("Layer 1##tiling", &terrain.layer_tiling[1], 1.0f, 256.0f);
						ImGui::SliderFloat("Layer 2##tiling", &terrain.layer_tiling[2], 1.0f, 256.0f);
						ImGui::SliderFloat("Layer 3##tiling", &terrain.layer_tiling[3], 1.0f, 256.0f);
						ImGui::TreePop();
					}

					if (ImGui::TreeNodeEx("Layer Textures", 0))
					{
						static Char const* filter_list = "jpg,jpeg,tga,dds,png";
						for (Uint32 i = 0; i < 4; ++i)
						{
							ImGui::PushID((Int32)(100 + i * 3));
							ImGui::Text("Layer %d", i);
							if (ImGui::Button("Albedo"))
							{
								nfdchar_t* file_path = NULL;
								if (NFD_OpenDialog(filter_list, NULL, &file_path) == NFD_OKAY)
								{
									terrain.layer_albedo[i] = g_TextureManager.LoadTexture(file_path, true);
									free(file_path);
								}
							}
							ImGui::SameLine();
							if (ImGui::Button("Normal"))
							{
								nfdchar_t* file_path = NULL;
								if (NFD_OpenDialog(filter_list, NULL, &file_path) == NFD_OKAY)
								{
									terrain.layer_normal[i] = g_TextureManager.LoadTexture(file_path);
									free(file_path);
								}
							}
							ImGui::SameLine();
							if (ImGui::Button("ARM"))
							{
								nfdchar_t* file_path = NULL;
								if (NFD_OpenDialog(filter_list, NULL, &file_path) == NFD_OKAY)
								{
									terrain.layer_arm[i] = g_TextureManager.LoadTexture(file_path);
									free(file_path);
								}
							}
							ImGui::PopID();
						}
						ImGui::TreePop();
					}

					if (needs_rebuild)
					{
						normals_generated = false;
					}
					if (ImGui::Button("Regenerate Heightmap"))
					{
						Heightmap hm(terrain.procedural_desc);
						CreateHeightmapTexture(hm);
						normals_generated = false;
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer);
	}

	void TerrainRenderer::OnResize(Uint32 w, Uint32 h)
	{
		width = w;
		height = h;
	}

	void TerrainRenderer::OnSceneInitialized()
	{
		gpu_resources_initialized = false;
		normals_generated = false;
	}

	void TerrainRenderer::InitializeGPUResources()
	{
		auto terrain_view = reg.view<Terrain>();
		if (terrain_view.empty()) 
		{
			return;
		}

		entt::entity terrain_entity = *terrain_view.begin();
		Terrain& terrain = terrain_view.get<Terrain>(terrain_entity);
		for (Uint32 i = 0; i < 4; ++i)
		{
			if (terrain.layer_albedo[i] == INVALID_TEXTURE_HANDLE)
				terrain.layer_albedo[i] = DEFAULT_WHITE_TEXTURE_HANDLE;
			if (terrain.layer_normal[i] == INVALID_TEXTURE_HANDLE)
				terrain.layer_normal[i] = DEFAULT_NORMAL_TEXTURE_HANDLE;
			if (terrain.layer_arm[i] == INVALID_TEXTURE_HANDLE)
				terrain.layer_arm[i] = DEFAULT_WHITE_TEXTURE_HANDLE;
		}

		if (!terrain.heightmap_path.empty())
		{
			Heightmap hm(terrain.heightmap_path);
			CreateHeightmapTexture(hm);
		}
		else
		{
			Heightmap hm(terrain.procedural_desc);
			CreateHeightmapTexture(hm);
		}

		CreateNormalmapTexture();

		// Create splatmap
		if (!terrain.splatmap_path.empty())
		{
			CreateSplatmapTexture(terrain.splatmap_path);
		}
		else
		{
			CreateDefaultSplatmap();
		}

		gpu_resources_initialized = true;
	}

	void TerrainRenderer::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		gfx_pso_desc.input_layout.elements.push_back(
			{ "POSITION", 0, GfxFormat::R32G32_FLOAT, 0, 0, GfxInputClassification::PerVertexData });
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_Terrain;
		gfx_pso_desc.PS = PS_Terrain;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.num_render_targets = 4;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.rtv_formats[1] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.rtv_formats[2] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.rtv_formats[3] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		terrain_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(gfx, gfx_pso_desc);

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_TerrainNormals;
		terrain_normals_pso = gfx->CreateManagedComputePipelineState(compute_pso_desc);
	}

	void TerrainRenderer::CreatePatchMesh()
	{
		Uint32 verts_per_side = PATCH_GRID_SIZE + 1;
		Uint32 vertex_count = verts_per_side * verts_per_side;
		std::vector<Vector2> vertices(vertex_count);
		for (Uint32 z = 0; z < verts_per_side; ++z)
		{
			for (Uint32 x = 0; x < verts_per_side; ++x)
			{
				Float fx = (Float)x / (Float)PATCH_GRID_SIZE;
				Float fz = (Float)z / (Float)PATCH_GRID_SIZE;
				vertices[z * verts_per_side + x] = Vector2(fx, fz);
			}
		}

		Uint32 quad_count = PATCH_GRID_SIZE * PATCH_GRID_SIZE;
		patch_index_count = quad_count * 6;
		std::vector<Uint32> indices(patch_index_count);
		Uint32 idx = 0;
		for (Uint32 z = 0; z < PATCH_GRID_SIZE; ++z)
		{
			for (Uint32 x = 0; x < PATCH_GRID_SIZE; ++x)
			{
				Uint32 tl = z * verts_per_side + x;
				Uint32 tr = tl + 1;
				Uint32 bl = (z + 1) * verts_per_side + x;
				Uint32 br = bl + 1;

				indices[idx++] = tl;
				indices[idx++] = bl;
				indices[idx++] = tr;

				indices[idx++] = tr;
				indices[idx++] = bl;
				indices[idx++] = br;
			}
		}
		GfxBufferDesc vb_desc = VertexBufferDesc(vertex_count, sizeof(Vector2), false);
		patch_vertex_buffer = gfx->CreateBuffer(vb_desc, vertices.data());
		patch_vertex_buffer->SetName("Terrain Patch VB");

		GfxBufferDesc ib_desc = IndexBufferDesc(patch_index_count, false, false);
		patch_index_buffer = gfx->CreateBuffer(ib_desc, indices.data());
		patch_index_buffer->SetName("Terrain Patch IB");
	}

	void TerrainRenderer::CreateHeightmapTexture(Heightmap const& hm)
	{
		heightmap_width = (Uint32)hm.Width();
		heightmap_depth = (Uint32)hm.Depth();
		std::vector<Float> heightmap_data(heightmap_width * heightmap_depth);
		for (Uint32 z = 0; z < heightmap_depth; ++z)
		{
			for (Uint32 x = 0; x < heightmap_width; ++x)
			{
				heightmap_data[z * heightmap_width + x] = hm.HeightAt(x, z);
			}
		}

		GfxTextureDesc tex_desc{};
		tex_desc.width = heightmap_width;
		tex_desc.height = heightmap_depth;
		tex_desc.format = GfxFormat::R32_FLOAT;
		tex_desc.bind_flags = GfxBindFlag::ShaderResource;
		tex_desc.initial_state = GfxResourceState::AllSRV;

		GfxTextureSubData sub_data{};
		sub_data.data = heightmap_data.data();
		sub_data.row_pitch = sizeof(Float) * heightmap_width;
		sub_data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &sub_data;
		init_data.sub_count = 1;

		heightmap_texture = gfx->CreateTexture(tex_desc, init_data);
		heightmap_texture->SetName("Terrain Heightmap");
		heightmap_srv = gfx->CreateTextureSRV(heightmap_texture.get());
	}

	void TerrainRenderer::CreateNormalmapTexture()
	{
		GfxTextureDesc tex_desc{};
		tex_desc.width = heightmap_width;
		tex_desc.height = heightmap_depth;
		tex_desc.format = GfxFormat::R8G8B8A8_UNORM;
		tex_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		tex_desc.initial_state = GfxResourceState::ComputeUAV;

		normalmap_texture = gfx->CreateTexture(tex_desc);
		normalmap_texture->SetName("Terrain Normalmap");
		normalmap_srv = gfx->CreateTextureSRV(normalmap_texture.get());
		normalmap_uav = gfx->CreateTextureUAV(normalmap_texture.get());
	}

	void TerrainRenderer::CreateSplatmapTexture(std::string const& path)
	{
		Image img(path);
		Uint32 w = img.Width();
		Uint32 h = img.Height();

		GfxTextureDesc tex_desc{};
		tex_desc.width = w;
		tex_desc.height = h;
		tex_desc.format = GfxFormat::R8G8B8A8_UNORM;
		tex_desc.bind_flags = GfxBindFlag::ShaderResource;
		tex_desc.initial_state = GfxResourceState::AllSRV;

		GfxTextureSubData sub_data{};
		sub_data.data = img.Data();
		sub_data.row_pitch = w * 4;
		sub_data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &sub_data;
		init_data.sub_count = 1;

		splatmap_texture = gfx->CreateTexture(tex_desc, init_data);
		splatmap_texture->SetName("Terrain Splatmap");
		splatmap_srv = gfx->CreateTextureSRV(splatmap_texture.get());
	}

	void TerrainRenderer::CreateDefaultSplatmap()
	{
		Uint32 splatmap_size = 256;
		std::vector<Uint32> splatmap_data(splatmap_size * splatmap_size, 0x000000FF);

		GfxTextureDesc tex_desc{};
		tex_desc.width = splatmap_size;
		tex_desc.height = splatmap_size;
		tex_desc.format = GfxFormat::R8G8B8A8_UNORM;
		tex_desc.bind_flags = GfxBindFlag::ShaderResource;
		tex_desc.initial_state = GfxResourceState::AllSRV;

		GfxTextureSubData sub_data{};
		sub_data.data = splatmap_data.data();
		sub_data.row_pitch = sizeof(Uint32) * splatmap_size;
		sub_data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &sub_data;
		init_data.sub_count = 1;

		splatmap_texture = gfx->CreateTexture(tex_desc, init_data);
		splatmap_texture->SetName("Terrain Splatmap");
		splatmap_srv = gfx->CreateTextureSRV(splatmap_texture.get());
	}

	void TerrainRenderer::BuildQuadtree(Float cam_x, Float cam_z, Float terrain_width, Float terrain_depth)
	{
		visible_patches.clear();
		Float half_w = terrain_width * 0.5f;
		Float half_d = terrain_depth * 0.5f;
		Float root_size = terrain_width > terrain_depth ? terrain_width : terrain_depth;
		SubdivideNode(cam_x, cam_z, -half_w, -half_d, root_size, 0, half_w, half_d);
	}

	void TerrainRenderer::SubdivideNode(Float cam_x, Float cam_z, Float node_x, Float node_z, Float node_size, Uint32 lod, Float terrain_half_w, Float terrain_half_d)
	{
		if (node_x + node_size < -terrain_half_w || node_x > terrain_half_w ||
			node_z + node_size < -terrain_half_d || node_z > terrain_half_d)
		{
			return;
		}

		Float center_x = node_x + node_size * 0.5f;
		Float center_z = node_z + node_size * 0.5f;
		Float dx = cam_x - center_x;
		Float dz = cam_z - center_z;
		Float distance = std::sqrt(dx * dx + dz * dz);

		Float base_distance = 64.0f;
		Float threshold = base_distance * (Float)(1 << lod);
		if (distance < threshold && lod < MAX_LOD)
		{
			Float child_size = node_size * 0.5f;
			SubdivideNode(cam_x, cam_z, node_x, node_z, child_size, lod + 1, terrain_half_w, terrain_half_d);
			SubdivideNode(cam_x, cam_z, node_x + child_size, node_z, child_size, lod + 1, terrain_half_w, terrain_half_d);
			SubdivideNode(cam_x, cam_z, node_x, node_z + child_size, child_size, lod + 1, terrain_half_w, terrain_half_d);
			SubdivideNode(cam_x, cam_z, node_x + child_size, node_z + child_size, child_size, lod + 1, terrain_half_w, terrain_half_d);
		}
		else
		{
			Float clamp_x = std::max(node_x, -terrain_half_w);
			Float clamp_z = std::max(node_z, -terrain_half_d);
			Float clamp_end_x = std::min(node_x + node_size, terrain_half_w);
			Float clamp_end_z = std::min(node_z + node_size, terrain_half_d);
			if (clamp_end_x - clamp_x > 0.0f && clamp_end_z - clamp_z > 0.0f)
			{
				TerrainQuadNode node{};
				node.x = clamp_x;
				node.z = clamp_z;
				node.size = node_size;
				node.lod = lod;
				visible_patches.push_back(node);
			}
		}
	}
}
