#import <Cocoa/Cocoa.h>
#include "Core/Engine.h"
#include "Core/FatalAssert.h"
#include "Core/CommandLineOptions.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Logging/FileSink.h"
#include "Logging/ConsoleSink.h"
#include "Editor/Editor.h"
#include "Utilities/CLIParser.h"

using namespace adria;

@interface AdriaAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AdriaAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return YES;
}

@end

int main(int argc, char* argv[])
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];

        AdriaAppDelegate* appDelegate = [[AdriaAppDelegate alloc] init];
        [NSApp setDelegate:appDelegate];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        CommandLineOptions::Initialize(argc, argv);

        std::string log_file = CommandLineOptions::GetLogFile();
        LogLevel log_level = static_cast<LogLevel>(CommandLineOptions::GetLogLevel());
        ADRIA_SINK(FileSink, log_file.c_str(), log_level);
        ADRIA_SINK(ConsoleSink, false, log_level); 
        
        WindowCreationParams window_params{};
        window_params.width = CommandLineOptions::GetWindowWidth();
        window_params.height = CommandLineOptions::GetWindowHeight();
        window_params.maximize = CommandLineOptions::GetMaximizeWindow();
        std::string window_title = CommandLineOptions::GetWindowTitle();
        window_params.title = window_title.c_str();

        Window window(window_params);
        g_Input.Initialize(&window);

        EditorInitParams editor_params{ .window = &window, .scene_file = CommandLineOptions::GetSceneFile() };
        g_Editor.Initialize(std::move(editor_params));
        window.GetWindowEvent().AddLambda([](WindowEventInfo const& msg_data) { g_Editor.OnWindowEvent(msg_data); });

        [NSApp activateIgnoringOtherApps:YES];
        while (window.Loop())
        {
            @autoreleasepool
            {
                g_Input.Tick();
                g_Editor.Run();
            }
        }

        g_Editor.Shutdown();
        [NSApp terminate:nil];
        exit(0);
    }

    return 0;
}
