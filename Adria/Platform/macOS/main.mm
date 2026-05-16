#import <Cocoa/Cocoa.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include "Core/Engine.h"
#include "Rendering/TriangleTestApp.h"
#include "Core/FatalAssert.h"
#include "Core/CommandLineOptions.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Logging/FileSink.h"
#include "Logging/ConsoleSink.h"
#include "Editor/Editor.h"
#include "Utilities/CLIParser.h"

using namespace adria;

static bool IsDebuggerAttached()
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info{};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    } 
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}

@interface AdriaAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, assign) adria::Window* window;
@end

@implementation AdriaAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    if (self.window)
    {
        self.window->Quit(0);
    }
    return NSTerminateCancel;
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

        NSMenu* menuBar = [[NSMenu alloc] init];

        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appMenuItem];
        NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Adria"];
        [appMenuItem setSubmenu:appMenu];
        [appMenu addItemWithTitle:@"Quit Adria" action:@selector(terminate:) keyEquivalent:@"q"];

        NSMenuItem* viewMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:viewMenuItem];
        NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
        [viewMenuItem setSubmenu:viewMenu];
        NSMenuItem* fullScreenItem = [viewMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
        [fullScreenItem setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];

        [NSApp setMainMenu:menuBar];

        CommandLineOptions::Initialize(argc, argv);

        if (CommandLineOptions::WaitDebugger())
        {
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Debug Attach"];
            [alert setInformativeText:@"Waiting for debugger. Click OK after attaching."];
            [alert addButtonWithTitle:@"OK"];
            while (!IsDebuggerAttached())
            {
                [alert runModal];
            }
        }

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
        appDelegate.window = &window;
        g_Input.Initialize(&window);

        [NSApp activateIgnoringOtherApps:YES];
        if (CommandLineOptions::GetTriangleTest())
        {
            TriangleTestApp app(&window);
            while (window.Loop())
            {
                @autoreleasepool
                {
                    app.Run();
                }
            }
        }
        else
        {
            EditorInitParams editor_params{ .window = &window, .scene_file = CommandLineOptions::GetSceneFile() };
            g_Editor.Initialize(std::move(editor_params));
            window.GetWindowEvent().AddLambda([](WindowEventInfo const& msg_data) { g_Editor.OnWindowEvent(msg_data); });
            while (window.Loop())
            {
                @autoreleasepool
                {
                    g_Editor.Run();
                }
            }
            g_Editor.Shutdown();
        }
    }

    return 0;
}
