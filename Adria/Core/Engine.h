#pragma once
#include "Graphics/GfxOptions.h"
#include "Rendering/ViewportData.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	struct WindowEventData;
	class Window;
	struct SceneConfig;
	class GfxDevice;
	class Renderer;
	class SceneLoader;
	struct EditorEvents;
	class ImGuiManager;
	class Camera;

	struct EngineInit
	{
		std::string scene_file = "scene.json";
		Window* window = nullptr;
		GfxOptions gfx_options;
	};

	class Engine
	{
		friend class Editor;

	public:
		explicit Engine(EngineInit const&);
		ADRIA_NONCOPYABLE_NONMOVABLE(Engine)
		~Engine();

		void OnWindowEvent(WindowEventData const& msg_data);
		void Run();

	private:
		Window* window = nullptr;
		entt::registry reg;
		std::unique_ptr<Camera> camera;
		std::unique_ptr<GfxDevice> gfx;
		std::unique_ptr<Renderer> renderer;
		std::unique_ptr<SceneLoader> scene_loader;
		ViewportData viewport_data;

	private:
		void InitializeScene(SceneConfig const&);
		void ProcessCVarIniFile(std::string const&);

		void Update(Float dt);
		void Render();

		void SetViewportData(ViewportData*);
		void RegisterEditorEventCallbacks(EditorEvents&);
	};
}