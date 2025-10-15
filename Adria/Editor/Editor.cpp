#include "nfd.h"
#include "IconsFontAwesome6.h"
#include "Editor.h"
#include "ImGuiManager.h"
#include "EditorSink.h"
#include "EditorConsole.h"
#include "Core/Engine.h"
#include "Core/Paths.h"
#include "Platform/Input.h"
#include "Rendering/Renderer.h"
#include "Rendering/Camera.h"
#include "Rendering/SceneLoader.h"
#include "Rendering/ShaderManager.h"
#include "Rendering/DebugRenderer.h"
#include "Rendering/HelperPasses.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/D3D12/D3D12Device.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxNsightPerfManager.h"
#include "Graphics/GfxRenderDoc.h"
#include "Graphics/GfxPIX.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/PathHelpers.h"
#include "Utilities/StringConversions.h"
#include "Utilities/Random.h"
#include "Math/BoundingVolumeUtil.h"

using namespace DirectX;
namespace fs = std::filesystem;

namespace adria
{
	extern Bool g_DumpRenderGraph;

	struct ProfilerState
	{
		Bool  show_average = false;
		struct AccumulatedTimeStamp
		{
			Float sum;
			Float minimum;
			Float maximum;

			AccumulatedTimeStamp()
				: sum(0.0f), minimum(FLT_MAX), maximum(0)
			{}
		};

		std::vector<AccumulatedTimeStamp> displayed_timestamps;
		std::vector<AccumulatedTimeStamp> accumulating_timestamps;
		Float64 last_reset_time = 0.0;
		Uint32 accumulating_frame_count = 0;
	};

	Editor::Editor() = default;
	Editor::~Editor() = default;

	void Editor::Initialize(EditorInitParams&& init)
	{
		editor_sink = ADRIA_SINK(EditorSink);
		engine = std::make_unique<Engine>(init.window, init.scene_file);
		gfx = engine->gfx.get();
		gui = CreateImguiManager(gfx);	
		engine->RegisterEditorEventCallbacks(editor_events);

		console = std::make_unique<EditorConsole>();
		ray_tracing_supported = gfx->GetCapabilities().SupportsRayTracing();
		selected_entity = entt::null;
		SetStyle_Default();
		fs::create_directory(paths::PixCapturesDir);
		fs::create_directory(paths::RenderDocCapturesDir);
	}
	void Editor::Shutdown()
	{
		gui.reset();
		engine.reset();
		console.reset();
	}
	void Editor::OnWindowEvent(WindowEventInfo const& msg_data)
	{
		if(engine) engine->OnWindowEvent(msg_data);
		if(gui) gui->OnWindowEvent(msg_data);
	}

	void Editor::Run()
	{
		HandleInput();
		if (gui->IsVisible())
		{
			engine->SetViewportData(&viewport_data);
		}
		else
		{
			engine->SetViewportData(nullptr);
		}

		engine->Run();

		if (reload_shaders)
		{
			gfx->WaitForGPU();
			ShaderManager::CheckIfShadersHaveChanged();
			reload_shaders = false;
		}
	}

	void Editor::EndFrame()
	{
		profiler_tree = g_GfxProfiler.GetProfilerTree();
	}

	Bool Editor::IsActive() const
	{
		return gui->IsVisible();
	}

	void Editor::AddCommand(GUICommand&& command)
	{
		commands.emplace_back(std::move(command));
	}
	void Editor::AddDebugTexture(GUITexture&& debug_texture)
	{
		debug_textures.emplace_back(std::move(debug_texture));
	}
	void Editor::AddRenderPass(RenderGraph& rg)
	{
		struct EditorPassData
		{
			RGTextureReadOnlyId src;
			RGRenderTargetId rt;
		};

		rg.AddPass<EditorPassData>("Editor Pass",
			[=](EditorPassData& data, RenderGraphBuilder& builder)
			{
				data.src = builder.ReadTexture(RG_NAME(FinalTexture));
				data.rt = builder.WriteRenderTarget(RG_NAME(Backbuffer), RGLoadStoreAccessOp::Preserve_Preserve);
				Vector2u display_resolution = engine->renderer->GetDisplayResolution();
				builder.SetViewport(display_resolution.x, display_resolution.y);
			},
			[=](EditorPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();

				GfxDescriptor src_descriptor = ctx.GetReadOnlyTexture(data.src);
				gui->Begin();
				{
					ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
					MenuBar();
					Scene(src_descriptor);
					ListEntities();
					AddEntities();
					Settings();
					Camera();
					Properties();
					Log();
					Console();
					Profiling();
					ShaderHotReload();
					Debug();
				}
				gui->End(cmd_list);
				commands.clear();
				debug_textures.clear();
			}, RGPassType::Graphics, RGPassFlags::ForceNoCull | RGPassFlags::LegacyRenderPass);

	}
	void Editor::HandleInput()
	{
		if (scene_focused && g_Input.IsKeyDown(KeyCode::I))
		{
			gui->ToggleVisibility();
			g_Input.SetMouseVisibility(gui->IsVisible());
		}
		if (g_Input.IsKeyDown(KeyCode::Tilde))
		{
			show_basic_console = !show_basic_console;
		}

		if (gui->IsVisible())
		{
			engine->camera->Enable(scene_focused);
		}
		else
		{
			engine->camera->Enable(true);
		}
	}
	void Editor::MenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu(ICON_FA_FILE" File"))
			{
				if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN" Open Scene"))
				{
					nfdchar_t* file_path = NULL;
					const nfdchar_t* filter_list = "json";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						SceneConfig scene_config{};
						if (ParseSceneConfig(file_path, scene_config, false))
						{
							engine->NewSceneRequest(scene_config);
						}
						free(file_path);
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(ICON_FA_WINDOW_MAXIMIZE " Windows"))
			{
				if (ImGui::MenuItem(ICON_FA_CLOCK" Profiler", 0, visibility_flags[Flag_Profiler]))			 visibility_flags[Flag_Profiler] = !visibility_flags[Flag_Profiler];
				if (ImGui::MenuItem(ICON_FA_COMMENT" Log", 0, visibility_flags[Flag_Log]))					 visibility_flags[Flag_Log] = !visibility_flags[Flag_Log];
				if (ImGui::MenuItem(ICON_FA_TERMINAL" Console ", 0, visibility_flags[Flag_Console]))		 visibility_flags[Flag_Console] = !visibility_flags[Flag_Console];
				if (ImGui::MenuItem(ICON_FA_CAMERA" Camera", 0, visibility_flags[Flag_Camera]))				 visibility_flags[Flag_Camera] = !visibility_flags[Flag_Camera];
				if (ImGui::MenuItem(ICON_FA_LIST " Entities", 0, visibility_flags[Flag_Entities]))			 visibility_flags[Flag_Entities] = !visibility_flags[Flag_Entities];
				if (ImGui::MenuItem(ICON_FA_FIRE" Hot Reload", 0, visibility_flags[Flag_HotReload]))		 visibility_flags[Flag_HotReload] = !visibility_flags[Flag_HotReload];
				if (ImGui::MenuItem(ICON_FA_GEAR" Settings", 0, visibility_flags[Flag_Settings]))			 visibility_flags[Flag_Settings] = !visibility_flags[Flag_Settings];
				if (ImGui::MenuItem(ICON_FA_BUG" Debug", 0, visibility_flags[Flag_Debug]))					 visibility_flags[Flag_Debug] = !visibility_flags[Flag_Debug];
				if (ImGui::MenuItem(ICON_FA_PLUS "Add Entities", 0, visibility_flags[Flag_AddEntities]))	 visibility_flags[Flag_AddEntities] = !visibility_flags[Flag_AddEntities];

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(ICON_FA_PAINTBRUSH" Themes"))
			{
				if (ImGui::MenuItem("Default", 0, theme == EditorTheme_Default))
				{
					if (theme != EditorTheme_Default)
					{
						theme = EditorTheme_Default;
						SetStyle_Default();
					}
				}
				if (ImGui::MenuItem("Cherry", 0, theme == EditorTheme_Cherry))
				{
					if (theme != EditorTheme_Cherry)
					{
						theme = EditorTheme_Cherry;
						SetStyle_Cherry();
					}
				}
				if (ImGui::MenuItem("Photoshop", 0, theme == EditorTheme_Photoshop))
				{
					if (theme != EditorTheme_Photoshop)
					{
						theme = EditorTheme_Photoshop;
						SetStyle_Photoshop();
					}
				}
				if (ImGui::MenuItem("Classic Steam", 0, theme == EditorTheme_ClassicSteam))
				{
					if (theme != EditorTheme_ClassicSteam)
					{
						theme = EditorTheme_ClassicSteam;
						SetStyle_ClassicSteam();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(ICON_FA_QUESTION" Help"))
			{
				ImGui::Text("TODO");
				ImGui::Spacing();
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void Editor::AddEntities()
	{
		if (!visibility_flags[Flag_AddEntities])
		{
			return;
		}

		if (ImGui::Begin("Add Entities", &visibility_flags[Flag_AddEntities]))
		{
			if (ImGui::TreeNodeEx("Point Lights", 0))
			{
				static Int light_count_to_add = 1;
				ImGui::SliderInt("Light Count", &light_count_to_add, 1, 128);
				if (ImGui::Button("Create Random Point Lights"))
				{
					static RealRandomGenerator real(0.0f, 1.0f);

					for (Int32 i = 0; i < light_count_to_add; ++i)
					{
						LightParameters light_params{};
						light_params.light_data.casts_shadows = false;
						light_params.light_data.color = Vector4(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = Vector4(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = Vector4(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
						light_params.light_data.type = LightType::Point;
						light_params.mesh_type = LightMesh::NoMesh;
						light_params.light_data.range = real() * 100.0f + 40.0f;
						light_params.light_data.active = true;
						light_params.light_data.volumetric = false;
						light_params.light_data.volumetric_strength = 0.004f;
						engine->scene_loader->LoadLight(light_params);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Spot Lights", 0))
			{
				static Int light_count_to_add = 1;
				ImGui::SliderInt("Light Count", &light_count_to_add, 1, 128);
				if (ImGui::Button("Create Random Spot Lights"))
				{
					static RealRandomGenerator real(0.0f, 1.0f);

					for (Int32 i = 0; i < light_count_to_add; ++i)
					{
						LightParameters light_params{};
						light_params.light_data.casts_shadows = false;
						light_params.light_data.inner_cosine = real();
						light_params.light_data.outer_cosine = real();
						light_params.light_data.color = Vector4(real() * 2, real() * 2, real() * 2, 1.0f);
						light_params.light_data.direction = Vector4(0.5f, -1.0f, 0.1f, 0.0f);
						light_params.light_data.position = Vector4(real() * 200 - 100, real() * 200.0f, real() * 200 - 100, 1.0f);
						light_params.light_data.type = LightType::Spot;
						light_params.mesh_type = LightMesh::NoMesh;
						light_params.light_data.range = real() * 100.0f + 40.0f;
						light_params.light_data.active = true;
						light_params.light_data.volumetric = false;
						light_params.light_data.volumetric_strength = 0.004f;
						if (light_params.light_data.inner_cosine > light_params.light_data.outer_cosine)
						{
							std::swap(light_params.light_data.inner_cosine, light_params.light_data.outer_cosine);
						}
						engine->scene_loader->LoadLight(light_params);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Ocean", 0))
			{
				static GridParameters ocean_params{};
				static Int32 tile_count[2] = { 512, 512 };
				static Float tile_size[2] = { 40.0f, 40.0f };
				static Float texture_scale[2] = { 20.0f, 20.0f };

				ImGui::SliderInt2("Tile Count", tile_count, 32, 1024);
				ImGui::SliderFloat2("Tile Size", tile_size, 1.0, 100.0f);
				ImGui::SliderFloat2("Texture Scale", texture_scale, 0.1f, 10.0f);

				ocean_params.tile_count_x = tile_count[0];
				ocean_params.tile_count_z = tile_count[1];
				ocean_params.tile_size_x = tile_size[0];
				ocean_params.tile_size_z = tile_size[1];
				ocean_params.texture_scale_x = texture_scale[0];
				ocean_params.texture_scale_z = texture_scale[1];

				if (ImGui::Button("Load Ocean"))
				{
					OceanParameters params{};
					params.ocean_grid = std::move(ocean_params);
					gfx->WaitForGPU();
					engine->scene_loader->LoadOcean(params);
				}

				if (ImGui::Button(ICON_FA_ERASER" Clear"))
				{
					for (auto e : engine->reg.view<Ocean>())
					{
						engine->reg.destroy(e);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNodeEx("Decals", 0))
			{
				static DecalParameters params{};
				static Char NAME_BUFFER[128];
				ImGui::InputText("Name", NAME_BUFFER, sizeof(NAME_BUFFER));
				params.name = std::string(NAME_BUFFER);
				ImGui::PushID(6);
				if (ImGui::Button("Select Albedo Texture"))
				{
					nfdchar_t* file_path = NULL;
					nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						std::string texture_path = file_path;
						params.albedo_texture_path = texture_path;
						free(file_path);
					}
				}
				ImGui::PopID();
				ImGui::Text(params.albedo_texture_path.c_str());

				ImGui::PushID(7);
				if (ImGui::Button("Select Normal Texture"))
				{
					nfdchar_t* file_path = NULL;
					nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
					nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
					if (result == NFD_OKAY)
					{
						std::string texture_path = file_path;
						params.normal_texture_path = texture_path;
						free(file_path);
					}
				}

				ImGui::PopID();
				ImGui::Text(params.normal_texture_path.c_str());

				ImGui::DragFloat("Size", &params.size, 2.0f, 10.0f, 200.0f);
				ImGui::DragFloat("Rotation", &params.rotation, 1.0f, -180.0f, 180.0f);
				ImGui::Checkbox("Modify GBuffer Normals", &params.modify_gbuffer_normals);

				PickingData const& picking_data = engine->renderer->GetPickingData();
				ImGui::Text("Picked Position: %f %f %f", picking_data.position.x, picking_data.position.y, picking_data.position.z);
				ImGui::Text("Picked Normal: %f %f %f", picking_data.normal.x, picking_data.normal.y, picking_data.normal.z);
				if (ImGui::Button("Load Decal"))
				{
					params.position = Vector3(picking_data.position);
					params.normal = Vector3(picking_data.normal);
					params.rotation = XMConvertToRadians(params.rotation);

					engine->scene_loader->LoadDecal(params);
				}
				if (ImGui::Button(ICON_FA_ERASER" Clear Decals"))
				{
					for (auto e : engine->reg.view<Decal>())
					{
						engine->reg.destroy(e);
					}
				}
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
		ImGui::End();
	}
	void Editor::ListEntities()
	{
		if (!visibility_flags[Flag_Entities]) 
		{
			return;
		}

		auto all_entities = engine->reg.view<Tag>();
		if (ImGui::Begin(ICON_FA_LIST" Entities ", &visibility_flags[Flag_Entities]))
		{
			std::vector<entt::entity> deleted_entities{};
			std::function<void(entt::entity, Bool)> ShowEntity;
			ShowEntity = [&](entt::entity e, Bool first_iteration)
			{
				Tag& tag = all_entities.get<Tag>(e);

				ImGuiTreeNodeFlags flags = ((selected_entity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
				flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
				Bool opened = ImGui::TreeNodeEx(tag.name.c_str(), flags);

				if (ImGui::IsItemClicked())
				{
					if (e == selected_entity)
					{
						selected_entity = entt::null;
					}
					else
					{
						selected_entity = e;
					}
				}

				if (opened)
				{
					ImGui::TreePop();
				}
			};
			for (auto e : all_entities)
			{
				ShowEntity(e, true);
			}
		}
		ImGui::End();
	}
	void Editor::Properties()
	{
		if (!visibility_flags[Flag_Entities])
		{
			return;
		}

		if (ImGui::Begin("Properties", &visibility_flags[Flag_Entities]))
		{
			GfxDevice* gfx = engine->gfx.get();
			if (selected_entity != entt::null)
			{
				Tag* tag = engine->reg.try_get<Tag>(selected_entity);
				if (tag)
				{
					Char buffer[256];
					memset(buffer, 0, sizeof(buffer));
					std::strncpy(buffer, tag->name.c_str(), sizeof(buffer));
					if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
					{
						tag->name = std::string(buffer);
					}
				}

				Light* light = engine->reg.try_get<Light>(selected_entity);
				if (light && ImGui::CollapsingHeader("Light"))
				{
					if (light->type == LightType::Directional)	{ ImGui::Text("Directional Light"); }
					else if (light->type == LightType::Spot)	{ ImGui::Text("Spot Light"); }
					else if (light->type == LightType::Point)	{ ImGui::Text("Point Light"); }

					Bool changed = false;
					Float color[3] = { light->color.x, light->color.y, light->color.z };
					changed |= ImGui::ColorEdit3("Light Color", color);
					light->color = Vector4(color[0], color[1], color[2], 1.0f);

					changed |= ImGui::SliderFloat("Light Intensity", &light->intensity, 0.0f, 50.0f);

					if (engine->reg.all_of<Material>(selected_entity))
					{
						Material& material = engine->reg.get<Material>(selected_entity);
						memcpy(material.albedo_color, color, 3 * sizeof(Float));
					}

					if (light->type == LightType::Directional || light->type == LightType::Spot)
					{
						Float direction[3] = { light->direction.x, light->direction.y, light->direction.z };
						changed |= ImGui::SliderFloat3("Light direction", direction, -1.0f, 1.0f);
						light->direction = Vector4(direction[0], direction[1], direction[2], 0.0f);
						if (light->type == LightType::Directional)
						{
							light->position = -light->direction * 1e3;
						}
					}

					if (light->type == LightType::Spot)
					{
						Float inner_angle = XMConvertToDegrees(acos(light->inner_cosine))
							, outer_angle = XMConvertToDegrees(acos(light->outer_cosine));
						changed |= ImGui::SliderFloat("Inner Spot Angle", &inner_angle, 0.0f, 90.0f);
						changed |= ImGui::SliderFloat("Outer Spot Angle", &outer_angle, inner_angle, 90.0f);

						light->inner_cosine = cos(XMConvertToRadians(inner_angle));
						light->outer_cosine = cos(XMConvertToRadians(outer_angle));
					}

					if (light->type == LightType::Point || light->type == LightType::Spot)
					{
						Float position[3] = { light->position.x,  light->position.y,  light->position.z };
						changed |= ImGui::SliderFloat3("Light position", position, -300.0f, 500.0f);
						light->position = Vector4(position[0], position[1], position[2], 1.0f);
						changed |= ImGui::SliderFloat("Range", &light->range, 50.0f, 1000.0f);
					}

					if (engine->reg.all_of<Transform>(selected_entity))
					{
						Transform& tr = engine->reg.get<Transform>(selected_entity);
						Vector3 translation(light->position.x, light->position.y, light->position.z);
						tr.current_transform = Matrix::CreateTranslation(translation);
					}
					ImGui::Checkbox("Active", &light->active);
					if (light->active && changed)
					{
						editor_events.light_changed_event.Broadcast();
					}

					if (light->type == LightType::Directional)
					{
						static Int current_shadow_type = light->casts_shadows;
						ImGui::Combo("Shadow Technique", &current_shadow_type, "None\0Shadow Map\0Ray Traced Shadows\0", 3);
						if (!ray_tracing_supported && current_shadow_type == 2) current_shadow_type = 1;

						light->casts_shadows = (current_shadow_type == 1);
						light->ray_traced_shadows = (current_shadow_type == 2);
					}
					else
					{
						ImGui::Checkbox("Casts Shadows", &light->casts_shadows);
					}

					if (light->casts_shadows)
					{
						if (light->type == LightType::Directional)
						{
							ImGui::Checkbox("Use Cascades", &light->use_cascades);
						}
					}

					ImGui::Checkbox("God Rays", &light->god_rays);
					if (light->god_rays)
					{
						ImGui::SliderFloat("God Rays Decay", &light->godrays_decay, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays Weight", &light->godrays_weight, 0.0f, 1.0f);
						ImGui::SliderFloat("God Rays Density", &light->godrays_density, 0.1f, 2.0f);
						ImGui::SliderFloat("God Rays Exposure", &light->godrays_exposure, 0.1f, 10.0f);
					}

					ImGui::Checkbox("Volumetric Lighting", &light->volumetric);
					if (light->volumetric)
					{
						ImGui::SliderFloat("Volumetric lighting Strength", &light->volumetric_strength, 0.0f, 0.1f);
					}
					ImGui::Checkbox("Lens Flare", &light->lens_flare);
				}

				Material* material = engine->reg.try_get<Material>(selected_entity);
				if (material && ImGui::CollapsingHeader("Material"))
				{
					ImGui::Text("Albedo Texture");
					if (material->albedo_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->albedo_texture);
						gui->ShowImage(tex_handle);
					}

					ImGui::PushID(0);
					if (ImGui::Button("Remove"))
					{
						material->albedo_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->albedo_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Metallic-Roughness Texture");
					if (material->metallic_roughness_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->metallic_roughness_texture);
						gui->ShowImage(tex_handle);
					}


					ImGui::PushID(1);
					if (ImGui::Button("Remove"))
					{
						material->metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->metallic_roughness_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Emissive Texture");
					if (material->emissive_texture != INVALID_TEXTURE_HANDLE)
					{
						GfxDescriptor tex_handle = g_TextureManager.GetSRV(material->emissive_texture);
						gui->ShowImage(tex_handle);
					}

					ImGui::PushID(2);
					if (ImGui::Button("Remove"))
					{
						material->emissive_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							material->emissive_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::ColorEdit3("Base Color", material->albedo_color);
					ImGui::SliderFloat("Metallic Factor", &material->metallic_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Roughness Factor", &material->roughness_factor, 0.0f, 1.0f);
					ImGui::SliderFloat("Emissive Factor", &material->emissive_factor, 0.0f, 32.0f);
				}

				Transform* transform = engine->reg.try_get<Transform>(selected_entity);
				if (transform && ImGui::CollapsingHeader("Transform"))
				{
					Matrix tr = transform->current_transform;
					
					Vector3 translation, scale;
					Quaternion rotation;
					Matrix(tr.m[0]).Decompose(scale, rotation, translation);
					Bool change = ImGui::InputFloat3("Translation", &translation.x);
					change &= ImGui::InputFloat3("Rotation", &rotation.x);
					change &= ImGui::InputFloat3("Scale", &scale.x);
					
					Matrix scale_matrix = Matrix::CreateScale(scale);
					Matrix rotation_matrix = Matrix::CreateFromQuaternion(rotation);
					Matrix translation_matrix = Matrix::CreateTranslation(translation);
					transform->current_transform = translation_matrix * rotation_matrix * scale_matrix;
				}

				Decal* decal = engine->reg.try_get<Decal>(selected_entity);
				if (decal && ImGui::CollapsingHeader("Decal"))
				{
					ImGui::Text("Decal Albedo Texture");
					GfxDescriptor tex_handle = g_TextureManager.GetSRV(decal->albedo_decal_texture);
					gui->ShowImage(tex_handle);

					ImGui::PushID(4);
					if (ImGui::Button("Remove"))
					{
						decal->albedo_decal_texture = INVALID_TEXTURE_HANDLE;
					}
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							decal->albedo_decal_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();

					ImGui::Text("Decal Normal Texture");
					tex_handle = g_TextureManager.GetSRV(decal->normal_decal_texture);
					gui->ShowImage(tex_handle);

					ImGui::PushID(5);
					if (ImGui::Button("Remove")) decal->normal_decal_texture = INVALID_TEXTURE_HANDLE;
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							decal->normal_decal_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
					ImGui::PopID();
					ImGui::Checkbox("Modify GBuffer Normals", &decal->modify_gbuffer_normals);
				}

				Skybox* skybox = engine->reg.try_get<Skybox>(selected_entity);
				if (skybox && ImGui::CollapsingHeader("Skybox"))
				{
					ImGui::Checkbox("Active", &skybox->active);
					if (ImGui::Button("Select"))
					{
						nfdchar_t* file_path = NULL;
						nfdchar_t const* filter_list = "jpg,jpeg,tga,dds,png";
						nfdresult_t result = NFD_OpenDialog(filter_list, NULL, &file_path);
						if (result == NFD_OKAY)
						{
							skybox->cubemap_texture = g_TextureManager.LoadTexture(file_path);
							free(file_path);
						}
					}
				}
			}
		}
		ImGui::End();
	}
	void Editor::Camera()
	{
		if (!visibility_flags[Flag_Camera])
		{
			return;
		}

		auto& camera = *engine->camera;
		if (ImGui::Begin(ICON_FA_CAMERA" Camera", &visibility_flags[Flag_Camera]))
		{
			Vector3 cam_pos = camera.Position();
			ImGui::SliderFloat3("Position", (Float*)&cam_pos, 0.0f, 2000.0f);
			camera.SetPosition(cam_pos);
			Float near_plane = camera.Near(), far_plane = camera.Far();
			Float fov = camera.Fov();
			ImGui::SliderFloat("Near", &near_plane, 10.0f, 3000.0f);
			ImGui::SliderFloat("Far", &far_plane, 0.001f, 2.0f);
			ImGui::SliderFloat("FOV", &fov, 0.01f, 1.5707f);
			camera.SetNearAndFar(near_plane, far_plane);
			camera.SetFov(fov);
			Vector3 look_at = camera.Forward();
			ImGui::Text("Look Vector: (%f,%f,%f)", look_at.x, look_at.y, look_at.z);
		}
		ImGui::End();
	}
	void Editor::Scene(GfxDescriptor& src)
	{
		ImGui::Begin(ICON_FA_GLOBE" Scene", nullptr, ImGuiWindowFlags_MenuBar);
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Lighting Path"))
				{
					LightingPath current_path = engine->renderer->GetLightingPath();
					auto AddMenuItem = [&](LightingPath lighting_path, Char const* item_name)
					{
						if (ImGui::MenuItem(item_name, nullptr, lighting_path == current_path)) { engine->renderer->SetLightingPath(lighting_path); }
					};
					#define AddLightingPathMenuItem(name) AddMenuItem(LightingPath::##name, #name)
					AddLightingPathMenuItem(Deferred);
					AddLightingPathMenuItem(TiledDeferred);
					AddLightingPathMenuItem(ClusteredDeferred);
					AddLightingPathMenuItem(PathTracing);
					#undef AddLightingPathMenuItem
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Debug View"))
				{
					RendererDebugView current_debug_view = engine->renderer->GetDebugView();
					auto AddMenuItem = [&](RendererDebugView output, Char const* item_name)
					{
						if (ImGui::MenuItem(item_name, nullptr, output == current_debug_view)) { engine->renderer->SetDebugView(output); }
					};

					#define AddDebugViewMenuItem(name) AddMenuItem(RendererDebugView::##name, #name)
					AddDebugViewMenuItem(Final);
					AddDebugViewMenuItem(Diffuse);
					AddDebugViewMenuItem(WorldNormal);
					AddDebugViewMenuItem(Depth);
					AddDebugViewMenuItem(Roughness);
					AddDebugViewMenuItem(Metallic);
					AddDebugViewMenuItem(Emissive);
					AddDebugViewMenuItem(MaterialID);
					AddDebugViewMenuItem(MeshletID);
					AddDebugViewMenuItem(AmbientOcclusion);
					AddDebugViewMenuItem(IndirectLighting);
					AddDebugViewMenuItem(Custom);
					AddDebugViewMenuItem(ShadingExtension);
					AddDebugViewMenuItem(ViewMipMaps);
					AddDebugViewMenuItem(TriangleOverdraw);
					AddDebugViewMenuItem(MotionVectors);
					#undef AddDebugViewMenuItem
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			ImVec2 v_min = ImGui::GetWindowContentRegionMin();
			ImVec2 v_max = ImGui::GetWindowContentRegionMax();
			v_min.x += ImGui::GetWindowPos().x;
			v_min.y += ImGui::GetWindowPos().y;
			v_max.x += ImGui::GetWindowPos().x;
			v_max.y += ImGui::GetWindowPos().y;
			ImVec2 size(v_max.x - v_min.x, v_max.y - v_min.y);
			gui->ShowImage(src, size);

			scene_focused = ImGui::IsWindowFocused();

			ImVec2 mouse_pos = ImGui::GetMousePos();
			viewport_data.mouse_position_x = mouse_pos.x;
			viewport_data.mouse_position_y = mouse_pos.y;
			viewport_data.scene_viewport_focused = scene_focused;
			viewport_data.scene_viewport_pos_x = v_min.x;
			viewport_data.scene_viewport_pos_y = v_min.y;
			viewport_data.scene_viewport_size_x = size.x;
			viewport_data.scene_viewport_size_y = size.y;
		}
		ImGui::End();
	}
	void Editor::Log()
	{
		if (!visibility_flags[Flag_Log])
		{
			return;
		}

		editor_sink->Draw(ICON_FA_COMMENT" Log", &visibility_flags[Flag_Log]);
	}
	void Editor::Console()
	{
		if (show_basic_console)
		{
			ImGui::SetNextWindowSize(ImVec2(viewport_data.scene_viewport_size_x, 65));
			ImGui::SetNextWindowPos(ImVec2(viewport_data.scene_viewport_pos_x, viewport_data.scene_viewport_pos_y + viewport_data.scene_viewport_size_y - 65));
			console->DrawBasic(ICON_FA_TERMINAL "BasicConsole ", nullptr);
		}

		if (!visibility_flags[Flag_Console]) 
		{
			return;
		}

		console->Draw(ICON_FA_TERMINAL "Console ", &visibility_flags[Flag_Console]);
	}

	void Editor::Settings()
	{
		if (!visibility_flags[Flag_Settings]) 
		{
			return;
		}

		std::array<std::vector<GUICommand*>, GUICommandGroup_Count> grouped_commands;
		for (GUICommand& cmd : commands)
		{
			grouped_commands[cmd.group].push_back(&cmd);
		}

		if (ImGui::Begin(ICON_FA_GEAR" Settings", &visibility_flags[Flag_Settings]))
		{
			for (Uint32 i = 0; i < GUICommandGroup_Count; ++i)
			{
				if (i != GUICommandGroup_None)
				{
					ImGui::SeparatorText(GUICommandGroupNames[i]);
				}
				std::array<std::vector<GUICommand*>, GUICommandSubGroup_Count> subgrouped_commands;
				for (GUICommand*& cmd : grouped_commands[i])
				{
					subgrouped_commands[cmd->subgroup].push_back(cmd);
				}

				for (Uint32 i = 0; i < GUICommandSubGroup_Count; ++i)
				{
					if (subgrouped_commands[i].empty())
					{
						continue;
					}

					if (i == GUICommandSubGroup_None)
					{
						for (GUICommand* cmd : subgrouped_commands[i])
						{
							cmd->callback();
						}
					}
					else
					{
						if (ImGui::TreeNode(GUICommandSubGroupNames[i]))
						{
							for (GUICommand* cmd : subgrouped_commands[i])
							{
								cmd->callback();
							}
							ImGui::TreePop();
						}
					}
				}

			}
		}
		ImGui::End();
	}
	void Editor::Profiling()
	{
		if (!visibility_flags[Flag_Profiler])
		{
			return;
		}

		if (ImGui::Begin(ICON_FA_CLOCK" Profiling", &visibility_flags[Flag_Profiler]))
		{
			ImGuiIO io = ImGui::GetIO();
#if GFX_PROFILING_USE_TRACY
			if (ImGui::Button("Run Tracy"))
			{
				static Char const* tracy_command = "start " SOLUTION_DIR"\\External\\tracy\\Tracy-0.11.1\\tracy-profiler.exe";
				system(tracy_command);
			}
#endif
			static Bool show_profiling = true;
			ImGui::Checkbox("Show Profiling Results", &show_profiling);
			if (show_profiling)
			{
				static constexpr Uint64 NUM_FRAMES = 128;
				static constexpr Int32 FRAME_TIME_GRAPH_MAX_FPS[] = { 800, 240, 120, 90, 65, 45, 30, 15, 10, 5, 4, 3, 2, 1 };

				static ProfilerState state{};
				static Float FrameTimeArray[NUM_FRAMES] = { 0 };
				static Float RecentHighestFrameTime = 0.0f;
				static Float FrameTimeGraphMaxValues[ARRAYSIZE(FRAME_TIME_GRAPH_MAX_FPS)] = { 0 };
				for (Uint64 i = 0; i < ARRAYSIZE(FrameTimeGraphMaxValues); ++i) 
				{ 
					FrameTimeGraphMaxValues[i] = 1000.f / FRAME_TIME_GRAPH_MAX_FPS[i]; 
				}

				FrameTimeArray[NUM_FRAMES - 1] = 1000.0f / io.Framerate;
				for (Uint32 i = 0; i < NUM_FRAMES - 1; i++)
				{
					FrameTimeArray[i] = FrameTimeArray[i + 1];
				}
				RecentHighestFrameTime = std::max(RecentHighestFrameTime, FrameTimeArray[NUM_FRAMES - 1]);

				Float frame_time_ms = FrameTimeArray[NUM_FRAMES - 1];
				Int32 const fps = static_cast<Int32>(1000.0f / frame_time_ms);
				ImGui::Text("FPS        : %d (%.2f ms)", fps, frame_time_ms);
#if GFX_PROFILING
				Uint32 const profiler_tree_size = (Uint32)profiler_tree->Size();
				if (ImGui::CollapsingHeader("Timings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Show Avg/Min/Max", &state.show_average);
					ImGui::Spacing();

					Uint64 max_i = 0;
					for (Uint64 i = 0; i < ARRAYSIZE(FrameTimeGraphMaxValues); ++i)
					{
						if (RecentHighestFrameTime < FrameTimeGraphMaxValues[i])
						{
							max_i = std::min(ARRAYSIZE(FrameTimeGraphMaxValues) - 1, i + 1);
							break;
						}
					}
					ImGui::PlotLines("GPU Profile Lines", FrameTimeArray, NUM_FRAMES, 0, "GPU frame time (ms)", 0.0f, FrameTimeGraphMaxValues[max_i], ImVec2(0, 80));

					constexpr Uint32 avg_timestamp_update_interval = 1000;
					static auto MillisecondsNow = []()
					{
						static LARGE_INTEGER s_frequency;
						static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
						Float64 milliseconds = 0;
						if (s_use_qpc)
						{
							LARGE_INTEGER now;
							QueryPerformanceCounter(&now);
							milliseconds = Float64(1000.0 * now.QuadPart) / s_frequency.QuadPart;
						}
						else milliseconds = Float64(GetTickCount64());
						return milliseconds;
					};
					const Float64 current_time = MillisecondsNow();

					Bool reset_accumulating_state = false;
					if ((state.accumulating_frame_count > 1) && ((current_time - state.last_reset_time) > avg_timestamp_update_interval))
					{
						std::swap(state.displayed_timestamps, state.accumulating_timestamps);
						for (Uint32 i = 0; i < state.displayed_timestamps.size(); i++)
						{
							state.displayed_timestamps[i].sum /= state.accumulating_frame_count;
						}
						reset_accumulating_state = true;
					}

					reset_accumulating_state |= (state.accumulating_timestamps.size() != profiler_tree_size);
					if (reset_accumulating_state)
					{
						state.accumulating_timestamps.resize(0);
						state.accumulating_timestamps.resize(profiler_tree_size);
						state.last_reset_time = current_time;
						state.accumulating_frame_count = 0;
					}

					struct ProfilerNodeState 
					{
						std::unordered_map<std::string, Bool> open_states;
						std::unordered_map<std::string, Uint64> node_ids;

						void* GetNodeId(Char const* name)
						{
							static Uint64 id = 0;
							if (!node_ids.contains(name))
							{
								node_ids[name] = id++;
							}
							return reinterpret_cast<void*>(node_ids[name]);
						}

						Bool IsNodeOpen(Char const* name) 
						{
							auto it = open_states.find(name);
							return it == open_states.end() ? true : it->second;
						}

						void ToggleNodeState(Char const* name)
						{
							open_states[name] = !IsNodeOpen(name);
						}
					};
					static ProfilerNodeState s_ProfilerNodeState;

					ImGui::BeginTable("Profiler", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg);
					ImGui::TableSetupColumn("Pass");
					ImGui::TableSetupColumn("Time");
					ImGui::TableHeadersRow();

					std::unordered_map<GfxProfilerTreeNode*, Bool> visible_nodes;
					profiler_tree->TraversePreOrder([&](GfxProfilerTreeNode* node)
						{
							if (node->GetParent() == nullptr) 
							{
								visible_nodes[node] = true;
								return;
							}
							GfxProfilerTreeNode* parent = node->GetParent();
							Bool parent_visible = visible_nodes[parent];
							Bool parent_expanded = s_ProfilerNodeState.IsNodeOpen(parent->GetName().data());
							visible_nodes[node] = parent_visible && parent_expanded;
						});

					profiler_tree->TraversePreOrder([&](GfxProfilerTreeNode* node)
						{
							if (!visible_nodes[node]) 
							{
								return;
							}
							std::string_view node_name = node->GetName();
							Float node_time = (Float)node->GetData().time;
							Uint32 i = node->GetData().index;

							Uint32 node_depth = node->GetDepth();
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);

							if (node_depth > 0)
							{
								ImGui::Indent(node_depth * 16.0f);
							}
							Bool is_open = s_ProfilerNodeState.IsNodeOpen(node_name.data());

							ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
							if (node->GetChildren().empty()) 
							{
								flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
							}
							if (is_open) 
							{
								flags |= ImGuiTreeNodeFlags_DefaultOpen;
							}

							void* node_id = s_ProfilerNodeState.GetNodeId(node_name.data());
							ImGui::PushID(node_id);
							Bool node_opened = ImGui::TreeNodeEx(node_name.data(), flags);
							if (ImGui::IsItemClicked() && !node->GetChildren().empty())
							{
								s_ProfilerNodeState.ToggleNodeState(node_name.data());
							}
							ImGui::PopID();

							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%.2f ms", node_time);
							if (state.show_average) 
							{
								if (state.displayed_timestamps.size() == profiler_tree_size) 
								{
									ImGui::SameLine();
									ImGui::Text("  avg: %.2f ms", state.displayed_timestamps[i].sum);
									ImGui::SameLine();
									ImGui::Text("  min: %.2f ms", state.displayed_timestamps[i].minimum);
									ImGui::SameLine();
									ImGui::Text("  max: %.2f ms", state.displayed_timestamps[i].maximum);
								}
								ProfilerState::AccumulatedTimeStamp* accumulating_timestamp = &state.accumulating_timestamps[i];
								accumulating_timestamp->sum += node_time;
								accumulating_timestamp->minimum = std::min<Float>(accumulating_timestamp->minimum, node_time);
								accumulating_timestamp->maximum = std::max<Float>(accumulating_timestamp->maximum, node_time);
							}
							if (node_depth > 0)
							{
								ImGui::Unindent(node_depth * 16.0f);
							}
							if (node_opened && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
							{
								ImGui::TreePop();
							}
						});
					ImGui::EndTable();
					state.accumulating_frame_count++;
				}
#endif
			}
#if defined(GFX_ENABLE_NV_PERF)
			if (GfxNsightPerfManager* nsight_perf_manager = gfx->GetNsightPerfManager())
			{
				static Bool display_nsight_perf = false;
				ImGui::Checkbox("Display subunit activity (Nsight Perf)", &display_nsight_perf);
				if (display_nsight_perf)
				{
					nsight_perf_manager->Render();
				}
			}
#endif
			static Bool display_vram_usage = false;
			ImGui::Checkbox("Display VRAM Usage", &display_vram_usage);
			if (display_vram_usage)
			{
				GPUMemoryUsage vram = gfx->GetMemoryUsage();
				Float const ratio = vram.usage * 1.0f / vram.budget;
				std::string vram_display_string = "VRAM usage: " + std::to_string(vram.usage / 1024 / 1024) + "MB / " + std::to_string(vram.budget / 1024 / 1024) + "MB\n";
				if (ratio >= 0.9f && ratio <= 1.0f)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
				}
				else if (ratio > 1.0f)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
				}
				ImGui::TextWrapped(vram_display_string.c_str());
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}
	void Editor::ShaderHotReload()
	{
		if (!visibility_flags[Flag_HotReload])
		{
			return;
		}
		if (ImGui::Begin(ICON_FA_FIRE" Shader Hot Reload", &visibility_flags[Flag_HotReload]))
		{
			if (ImGui::Button("Compile Changed Shaders")) reload_shaders = true;
		}
		ImGui::End();
	}
	void Editor::Debug()
	{
		if (!visibility_flags[Flag_Debug])
		{
			return;
		}

		if(ImGui::Begin(ICON_FA_BUG" Debug", &visibility_flags[Flag_Debug]))
		{
			if (ImGui::TreeNode("Debug Renderer"))
			{
				enum DebugRendererPrimitive
				{
					Line,
					Ray,
					Box,
					Sphere
				};
				static Int current_debug_renderer_primitive = 0;
				static Float debug_color[4] = { 0.0f,0.0f, 0.0f, 1.0f };
				ImGui::Combo("Debug Renderer Primitive", &current_debug_renderer_primitive, "Line\0Ray\0Box\0Sphere\0", 4);
				ImGui::ColorEdit3("Debug Color", debug_color);

				g_DebugRenderer.SetMode(DebugRendererMode::Persistent);
				switch (current_debug_renderer_primitive)
				{
				case Line:
				{
					static Float start[3] = { 0.0f };
					static Float end[3] = { 0.0f };
					ImGui::InputFloat3("Line Start", start);
					ImGui::InputFloat3("Line End", end);
					if (ImGui::Button("Add")) g_DebugRenderer.AddLine(Vector3(start), Vector3(end), Color(debug_color));
				}
				break;
				case Ray:
				{
					static Float origin[3] = { 0.0f };
					static Float dir[3] = { 0.0f };
					ImGui::InputFloat3("Ray Origin", origin);
					ImGui::InputFloat3("Ray Direction", dir);
					if (ImGui::Button("Add")) g_DebugRenderer.AddRay(Vector3(origin), Vector3(dir), Color(debug_color));
				}
				break;
				case Box:
				{
					static Float center[3] = { 0.0f };
					static Float extents[3] = { 0.0f };
					static Bool wireframe = false;
					ImGui::InputFloat3("Box Center", center);
					ImGui::InputFloat3("Box Extents", extents);
					ImGui::Checkbox("Wireframe", &wireframe);
					if (ImGui::Button("Add")) g_DebugRenderer.AddBox(Vector3(center), Vector3(extents), Color(debug_color), wireframe);
				}
				break;
				case Sphere:
				{
					static Float center[3] = { 0.0f };
					static Float radius = 1.0f;
					static Bool wireframe = false;
					ImGui::InputFloat3("Sphere Center", center);
					ImGui::InputFloat("Sphere Radius", &radius);
					ImGui::Checkbox("Wireframe", &wireframe);
					if (ImGui::Button("Add")) g_DebugRenderer.AddSphere(Vector3(center), radius, Color(debug_color), wireframe);
				}
				break;
				}
				g_DebugRenderer.SetMode(DebugRendererMode::Transient);

				if (ImGui::Button("Clear")) g_DebugRenderer.ClearPersistent();
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Render Graph"))
			{
				g_DumpRenderGraph = ImGui::Button("Dump render graph");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Screenshot"))
			{
				static Char filename[32] = "screenshot";
				ImGui::InputText("File name", filename, sizeof(filename));
				if (ImGui::Button("Take Screenshot"))
				{
					editor_events.take_screenshot_event.Broadcast(filename);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("PIX"))
			{
				static Char capture_name[32] = { 'a', 'd', 'r', 'i', 'a' };
				ImGui::InputText("Capture name", capture_name, sizeof(capture_name));

				static Int frame_count = 1;
				ImGui::SliderInt("Number of capture frames", &frame_count, 1, 10);

				if (ImGui::Button("Take capture"))
				{
					std::string capture_full_path = paths::PixCapturesDir + capture_name;
					GFX_PIX_TAKE_CAPTURE(capture_full_path.c_str(), frame_count);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("RenderDoc"))
			{
				static Char capture_name[32] = { 'a', 'd', 'r', 'i', 'a' };
				ImGui::InputText("Capture name", capture_name, sizeof(capture_name));

				static Int frame_count = 1;
				ImGui::SliderInt("Number of capture frames", &frame_count, 1, 10);

				if (ImGui::Button("Take capture"))
				{
					std::string capture_full_path = paths::RenderDocCapturesDir + capture_name;
					GFX_RENDERDOC_SETCAPFILE(capture_full_path.c_str());
					GFX_RENDERDOC_MULTIFRAMECAPTURE(frame_count);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Textures"))
			{
				struct VoidPointerHash
				{
					Uint64 operator()(void const* ptr) const { return reinterpret_cast<Uint64>(ptr); }
				};
				static std::unordered_map<void const*, GfxDescriptor, VoidPointerHash> debug_srv_map;

				for (Int32 i = 0; i < debug_textures.size(); ++i)
				{
					ImGui::PushID(i);
					auto& debug_texture = debug_textures[i];
					ImGui::Text(debug_texture.name);

					if (!debug_srv_map.contains(debug_texture.gfx_texture))
					{
						GfxDescriptor debug_srv_cpu = gfx->CreateTextureSRV(debug_texture.gfx_texture);
						debug_srv_map[debug_texture.gfx_texture] = debug_srv_cpu;
					}
					GfxDescriptor debug_srv_cpu = debug_srv_map[debug_texture.gfx_texture];
					Uint32 const width = debug_texture.gfx_texture->GetDesc().width;
					Uint32 const height = debug_texture.gfx_texture->GetDesc().height;
					Float const window_width = ImGui::GetWindowWidth();
					gui->ShowImage(debug_srv_cpu, ImVec2(window_width * 0.9f, window_width * 0.9f * (Float)height / width));
					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (GfxNsightPerfManager* nsight_perf_manager = gfx->GetNsightPerfManager())
			{
				if (ImGui::TreeNode("Nsight Perf Report"))
				{
					if (ImGui::Button("Generate Report"))
					{
						nsight_perf_manager->GenerateReport();
					}
					ImGui::TreePop();
				}
			}
			
		}
		ImGui::End();
	}

	void Editor::SetStyle_Default()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::StyleColorsDark(&style);

		style.Alpha = 1.0f;
		style.FrameRounding = 3.0f;
		style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

		for (Int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			Float H, S, V;
			ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

			if (S < 0.1f)
			{
				V = 1.0f - V;
			}
			ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
			if (col.w < 1.00f)
			{
				col.w *= 0.9f;
			}
		}
	}
	void Editor::SetStyle_Cherry()
	{
		//https://github.com/Patitotective/ImThemes/blob/main/themes.toml
		//name = "Cherry"
		//author = "r-lyeh"
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::StyleColorsDark(&style);

		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.6f;
		style.WindowPadding = ImVec2(6.0f, 3.0f);
		style.WindowRounding = 0.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(32.0f, 32.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ChildRounding = 0.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(5.0f, 1.0f);
		style.FrameRounding = 3.0f;
		style.FrameBorderSize = 1.0f;
		style.ItemSpacing = ImVec2(7.0f, 1.0f);
		style.ItemInnerSpacing = ImVec2(1.0f, 1.0f);
		style.CellPadding = ImVec2(4.0f, 2.0f);
		style.IndentSpacing = 6.0f;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 13.0f;
		style.ScrollbarRounding = 16.0f;
		style.GrabMinSize = 20.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 4.0f;
		style.TabBorderSize = 1.0f;
		style.TabMinWidthForCloseButton = 0.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(0.858f, 0.929f, 0.886f, 0.88f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.858f, 0.929f, 0.886f, 0.28f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.129f, 0.137f, 0.169f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.200f, 0.219f, 0.267f, 0.90f);
		colors[ImGuiCol_Border] = ImVec4(0.537f, 0.478f, 0.255f, 0.162f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.200f, 0.219f, 0.267f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.454f, 0.196f, 0.298f, 0.78f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.231f, 0.200f, 0.271f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.502f, 0.075f, 0.255f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.200f, 0.219f, 0.267f, 0.75f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.200f, 0.219f, 0.267f, 0.47f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.200f, 0.219f, 0.267f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.086f, 0.149f, 0.157f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.454f, 0.196f, 0.298f, 0.78f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.710f, 0.220f, 0.267f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.467f, 0.769f, 0.827f, 0.14f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.710f, 0.220f, 0.267f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.467f, 0.769f, 0.827f, 0.14f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.454f, 0.196f, 0.298f, 0.86f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.454f, 0.196f, 0.298f, 0.76f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.454f, 0.196f, 0.298f, 0.86f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.502f, 0.075f, 0.255f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.427f, 0.427f, 0.498f, 0.50f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.098f, 0.400f, 0.749f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.098f, 0.400f, 0.749f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.467f, 0.769f, 0.827f, 0.04f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.454f, 0.196f, 0.298f, 0.78f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.176f, 0.349f, 0.576f, 0.862f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.259f, 0.588f, 0.976f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.196f, 0.408f, 0.678f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.067f, 0.102f, 0.145f, 0.972f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.133f, 0.259f, 0.424f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.858f, 0.929f, 0.886f, 0.63f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.858f, 0.929f, 0.886f, 0.63f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.454f, 0.196f, 0.298f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.188f, 0.188f, 0.200f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.310f, 0.310f, 0.349f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.227f, 0.227f, 0.247f, 1.00f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.000f, 1.000f, 1.000f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.454f, 0.196f, 0.298f, 0.43f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.000f, 1.000f, 0.000f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.259f, 0.588f, 0.976f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800f, 0.800f, 0.800f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800f, 0.800f, 0.800f, 0.35f);
	}
	void Editor::SetStyle_Photoshop()
	{
		//https://github.com/Patitotective/ImThemes/blob/main/themes.toml
		//name = "Photoshop"
		//author = "Derydoca"
		ImGuiStyle& style = ImGui::GetStyle();

		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.6f;
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.WindowRounding = 4.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(32.0f, 32.0f);
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ChildRounding = 4.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 2.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(4.0f, 3.0f);
		style.FrameRounding = 2.0f;
		style.FrameBorderSize = 1.0f;
		style.ItemSpacing = ImVec2(8.0f, 4.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.CellPadding = ImVec2(4.0f, 2.0f);
		style.IndentSpacing = 21.0f;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 13.0f;
		style.ScrollbarRounding = 12.0f;
		style.GrabMinSize = 7.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		style.TabBorderSize = 1.0f;
		style.TabMinWidthForCloseButton = 0.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.28f, 0.28f, 0.28f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.156f);
		colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.391f);
		colors[ImGuiCol_Header] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.156f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.39f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.586f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.586f);
	}
	void Editor::SetStyle_ClassicSteam()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.6f;
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.WindowRounding = 0.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(32.0f, 32.0f);
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Left;
		style.ChildRounding = 0.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(4.0f, 3.0f);
		style.FrameRounding = 0.0f;
		style.FrameBorderSize = 1.0f;
		style.ItemSpacing = ImVec2(8.0f, 4.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.CellPadding = ImVec2(4.0f, 2.0f);
		style.IndentSpacing = 21.0f;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 14.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabMinSize = 10.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		style.TabBorderSize = 0.0f;
		style.TabMinWidthForCloseButton = 0.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.14f, 0.16f, 0.11f, 0.52f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.30f, 0.23f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.34f, 0.26f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.32f, 0.24f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.30f, 0.22f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.23f, 0.27f, 0.21f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
		colors[ImGuiCol_Button] = ImVec4(0.29f, 0.34f, 0.26f, 0.40f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
		colors[ImGuiCol_Header] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.42f, 0.31f, 0.60f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
		colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.11f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.19f, 0.23f, 0.18f, 0.00f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.54f, 0.57f, 0.51f, 0.78f);
		colors[ImGuiCol_TabActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.78f, 0.28f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.73f, 0.67f, 0.24f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	}
}
