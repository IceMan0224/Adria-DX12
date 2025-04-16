#include "Core/Window.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/CommandLineOptions.h"
#include "Core/LogSinks/FileSink.h"
#include "Core/LogSinks/DebuggerSink.h"
#include "Editor/Editor.h"
#include "Utilities/MemoryDebugger.h"
#include "Utilities/CLIParser.h"

using namespace adria;

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    CommandLineOptions::Initialize(lpCmdLine);
    
    std::string log_file = CommandLineOptions::GetLogFile();  
    LogLevel log_level = static_cast<LogLevel>(CommandLineOptions::GetLogLevel());
    ADRIA_SINK(FileSink, log_file.c_str(), log_level);
    ADRIA_SINK(DebuggerSink, log_level);

    WindowInit window_init{};
    window_init.width = CommandLineOptions::GetWindowWidth();
    window_init.height = CommandLineOptions::GetWindowHeight();
    window_init.maximize = CommandLineOptions::GetMaximizeWindow();
	std::string window_title = CommandLineOptions::GetWindowTitle();
	window_init.title = window_title.c_str();
    Window window(window_init);
    g_Input.Initialize(&window);

    EditorInit editor_init{ .window = &window, .scene_file = CommandLineOptions::GetSceneFile() };
    g_Editor.Init(std::move(editor_init));
    window.GetWindowEvent().AddLambda([](WindowEventInfo const& msg_data) { g_Editor.OnWindowEvent(msg_data); });
    while (window.Loop())
    {
        g_Editor.Run();
    }
    g_Editor.Destroy();
}



