#pragma once
#include "GUICommand.h"
#include "EditorEvents.h"
#include "ImGuizmo.h"
#include "Graphics/GfxTimestampProfilerFwd.h"
#include "Rendering/TextureHandle.h"
#include "Rendering/ViewportData.h"
#include "Utilities/Singleton.h"
#include "entt/entity/fwd.hpp"
#include "entt/entity/entity.hpp"

namespace adria
{
	class Window;
	class GfxDevice;
	class GfxDescriptor;
	class Engine;
	class ImGuiManager;
	class RenderGraph;
	class EditorSink;
	class EditorConsole;
	struct Material;

	struct EditorInitParams
	{
		Window* window;
		std::string scene_file;
	};

	class Editor : public Singleton<Editor>
	{
		friend class Singleton<Editor>;

		enum VisibilityFlag
		{
			Flag_Profiler,
			Flag_Camera,
			Flag_Log,
			Flag_Console,
			Flag_Entities,
			Flag_HotReload,
			Flag_Debug,
			Flag_Settings,
			Flag_Spawn,
			Flag_Count
		};

		enum EditorTheme
		{
			EditorTheme_Default,
			EditorTheme_Cherry,
			EditorTheme_Photoshop,
			EditorTheme_ClassicSteam,
		};

	public:
		void Initialize(EditorInitParams&& init);
		void Shutdown();

		void OnWindowEvent(WindowEventInfo const& msg_data);
		void OnSceneInitialized();
		void Run();
		void EndFrame();
		Bool IsActive() const;

		void AddCommand(GUICommand&& command);
		void AddDebugTexture(GUITexture&& debug_texture);
		void AddRenderPass(RenderGraph& rg);

		Engine*		GetEngine() const { return engine.get(); }
		entt::entity GetSelectedEntity() const { return primary_selected; }
		std::vector<entt::entity> const& GetSelectedEntities() const { return selected_entities; }
		Bool         IsSelected(entt::entity e) const;
		Bool         IsSelectionModeEnabled() const { return selection_mode; }
		void QueueEntityDestruction(entt::entity e) { pending_deletions.push_back(e); }

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<ImGuiManager> gui;
		GfxDevice* gfx;
		Bool ray_tracing_supported = false;

		std::unique_ptr<EditorConsole> console;
		EditorSink* editor_sink;

		Bool scene_focused = false;
		std::vector<entt::entity> selected_entities;
		entt::entity primary_selected = entt::null;
		Bool scroll_to_selected = false;
		Bool selection_mode = false;

		Bool reload_shaders = false;
		Bool visibility_flags[Flag_Count] = {false};
		EditorTheme theme = EditorTheme_Default;
		std::vector<GUICommand> commands;
		std::vector<GUITexture> debug_textures;
		
		EditorEvents editor_events;
		ViewportData viewport_data;
		Bool show_basic_console = false;
		std::vector<entt::entity> pending_deletions;

		GfxProfilerTree const* profiler_tree = nullptr;

		ImGuizmo::OPERATION gizmo_operation = ImGuizmo::TRANSLATE;
		ImGuizmo::MODE gizmo_mode = ImGuizmo::WORLD;
		Bool use_snap = false;
		Float snap_value[3] = {1.0f, 1.0f, 1.0f};

		Bool show_light_icons = true;
		TextureHandle directional_light_icon = INVALID_TEXTURE_HANDLE;
		TextureHandle point_light_icon = INVALID_TEXTURE_HANDLE;
		TextureHandle spot_light_icon = INVALID_TEXTURE_HANDLE;

		TextureHandle translate_icon = INVALID_TEXTURE_HANDLE;
		TextureHandle rotate_icon = INVALID_TEXTURE_HANDLE;
		TextureHandle scale_icon = INVALID_TEXTURE_HANDLE;

	private:
		Editor();
		~Editor();

		void HandleInput();
		void MenuBar();
		void AddEntities();
		void ListEntities();
		void Properties();
		void Camera();
		void Scene(GfxTexture const&);
		void Log();
		void Console();
		void Settings();
		void Profiling();
		void ShaderHotReload();
		void Debug();

		void SetStyle_Default();
		void SetStyle_Cherry();
		void SetStyle_Photoshop();
		void SetStyle_ClassicSteam();

		void SaveState();
		void LoadState();
		void FlushPendingDeletions();

		void GatherSubtree(entt::entity root, std::vector<entt::entity>& out) const;
		void ApplySelectionPick(entt::entity picked, Bool additive);
		void ClearSelection();
	};
	#define g_Editor Editor::Get()

}

