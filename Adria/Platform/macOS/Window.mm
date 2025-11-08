#import <Cocoa/Cocoa.h>
#include "Platform/Window.h"
#include "Core/Paths.h"

@interface AdriaWindowDelegate : NSObject <NSWindowDelegate>
{
    adria::Window* window;
}
- (instancetype)initWithWindow:(adria::Window*)w;
@end

@implementation AdriaWindowDelegate

- (instancetype)initWithWindow:(adria::Window*)w
{
    self = [super init];
    if (self)
    {
        window = w;
    }
    return self;
}

- (void)windowWillClose:(NSNotification*)notification
{
    window->Quit(0);
}

- (void)windowDidResize:(NSNotification*)notification
{
    NSWindow* nsWindow = [notification object];
    NSRect contentRect = [[nsWindow contentView] frame];

    adria::WindowEventInfo event_info{};
    event_info.handle = (__bridge void*)nsWindow;
    event_info.msg = 0; 
    event_info.wparam = 0;
    event_info.lparam = 0;
    event_info.width = static_cast<adria::Float>(contentRect.size.width);
    event_info.height = static_cast<adria::Float>(contentRect.size.height);

    window->BroadcastEvent(event_info);
}

@end

namespace adria
{
    Window::Window(WindowCreationParams const& init)
    {
        @autoreleasepool
        {
            NSRect frame = NSMakeRect(0, 0, init.width, init.height);

            NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                          NSWindowStyleMaskClosable |
                                          NSWindowStyleMaskResizable;

            NSWindow* nsWindow = [[NSWindow alloc] initWithContentRect:frame
                                                              styleMask:styleMask
                                                                backing:NSBackingStoreBuffered
                                                                  defer:NO];

            [nsWindow setTitle:@(init.title)];

            NSView* contentView = [[NSView alloc] initWithFrame:frame];
            [nsWindow setContentView:contentView];

            [nsWindow center];

            AdriaWindowDelegate* delegate = [[AdriaWindowDelegate alloc] initWithWindow:this];
            [nsWindow setDelegate:delegate];

            if (init.maximize)
            {
                [nsWindow zoom:nil];
            }

            [nsWindow makeKeyAndOrderFront:nil];

            std::string iconPath = paths::IconsDir + "adria_logo.icns";
            NSString* iconPathNS = [NSString stringWithUTF8String:iconPath.c_str()];
            NSImage* appIcon = [[NSImage alloc] initWithContentsOfFile:iconPathNS];
            if (appIcon)
            {
                [[NSApplication sharedApplication] setApplicationIconImage:appIcon];
            }

            window_handle = (__bridge void*)nsWindow;
        }
    }

    Window::~Window()
    {
        if (window_handle)
        {
            @autoreleasepool
            {
                NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
                [nsWindow close];
                window_handle = nullptr;
            }
        }
    }

    Uint32 Window::Width() const
    {
        if (!window_handle) return 0;

        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            NSRect contentRect = [[nsWindow contentView] frame];
            return static_cast<Uint32>(contentRect.size.width);
        }
    }

    Uint32 Window::Height() const
    {
        if (!window_handle) return 0;

        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            NSRect contentRect = [[nsWindow contentView] frame];
            return static_cast<Uint32>(contentRect.size.height);
        }
    }

    Uint32 Window::PositionX() const
    {
        if (!window_handle) return 0;

        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            NSRect frame = [nsWindow frame];
            return static_cast<Uint32>(frame.origin.x);
        }
    }

    Uint32 Window::PositionY() const
    {
        if (!window_handle) return 0;

        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            NSRect frame = [nsWindow frame];
            return static_cast<Uint32>(frame.origin.y);
        }
    }

    Bool Window::Loop()
    {
        @autoreleasepool
        {
            NSEvent* event;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES]))
            {
                [NSApp sendEvent:event];
            }

            return !should_quit;
        }
    }

    void Window::Quit(Int32 exit_code)
    {
        should_quit = true;
    }

    void* Window::Handle() const
    {
        return window_handle;
    }

    Bool Window::IsActive() const
    {
        if (!window_handle) return false;

        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window_handle;
            return [nsWindow isKeyWindow];
        }
    }

    void Window::BroadcastEvent(WindowEventInfo const& data)
    {
        window_event.Broadcast(data);
    }
}
