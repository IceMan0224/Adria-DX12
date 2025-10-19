#pragma once
#include "GUICommand.h"
#include "EditorEvents.h"
#include "Graphics/GfxTimestampProfilerFwd.h"
#include "Rendering/ViewportData.h"
#include "Utilities/Singleton.h"
#include "entt/entity/fwd.hpp"

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
			Flag_AddEntities,
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
		void Run();
		void EndFrame();
		Bool IsActive() const;

		void AddCommand(GUICommand&& command);
		void AddDebugTexture(GUITexture&& debug_texture);
		void AddRenderPass(RenderGraph& rg);

		Engine*		GetEngine() const { return engine.get(); }

	private:
		std::unique_ptr<Engine> engine;
		std::unique_ptr<ImGuiManager> gui;
		GfxDevice* gfx;
		Bool ray_tracing_supported = false;

		std::unique_ptr<EditorConsole> console;
		EditorSink* editor_sink;

		Bool scene_focused = false;
		entt::entity selected_entity;

		Bool reload_shaders = false;
		Bool visibility_flags[Flag_Count] = {false};
		EditorTheme theme = EditorTheme_Default;
		std::vector<GUICommand> commands;
		std::vector<GUITexture> debug_textures;
		
		EditorEvents editor_events;
		ViewportData viewport_data;
		Bool show_basic_console = false;

		GfxProfilerTree const* profiler_tree = nullptr;

	private:
		Editor();
		~Editor();

		void HandleInput();
		void MenuBar();
		void AddEntities();
		void ListEntities();
		void Properties();
		void Camera();
		void Scene(GfxDescriptor& tex);
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
	};
	#define g_Editor Editor::Get()

}

