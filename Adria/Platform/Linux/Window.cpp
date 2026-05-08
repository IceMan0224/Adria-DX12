#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "Platform/Window.h"

namespace adria
{
	struct XcbWindowHandle
	{
		xcb_connection_t* connection;
		xcb_window_t      window;
	};

	struct WindowImpl
	{
		XcbWindowHandle   xcb{};
		xcb_screen_t*     screen      = nullptr;
		xcb_atom_t        wm_delete   = XCB_NONE;
		Bool              should_quit = false;
		Uint32            width       = 0;
		Uint32            height      = 0;
		WindowEvent*      event       = nullptr;
	};

	Window::Window(WindowCreationParams const& params)
	{
		window_handle  = new XcbWindowHandle{};
		window_delegate = new WindowImpl{};

		WindowImpl* impl = (WindowImpl*)window_delegate;
		XcbWindowHandle* xcb = (XcbWindowHandle*)window_handle;

		Int32 screen_num = 0;
		xcb->connection = xcb_connect(nullptr, &screen_num);
		ADRIA_FATAL_ASSERT(!xcb_connection_has_error(xcb->connection), "Failed to connect to X11 display");

		xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(xcb->connection));
		for (Int32 i = 0; i < screen_num; ++i) { xcb_screen_next(&it); }
		impl->screen = it.data;
		impl->width  = params.width;
		impl->height = params.height;
		impl->event  = &window_event;

		xcb->window = xcb_generate_id(xcb->connection);
		Uint32 event_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
			| XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
			| XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

		Uint32 value_list[] = { impl->screen->black_pixel, event_mask };
		xcb_create_window(xcb->connection,
			XCB_COPY_FROM_PARENT, xcb->window, impl->screen->root,
			0, 0, (Uint16)params.width, (Uint16)params.height, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, impl->screen->root_visual,
			XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, value_list);

		xcb_change_property(xcb->connection, XCB_PROP_MODE_REPLACE, xcb->window,
			XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
			(Uint32)strlen(params.title), params.title);

		xcb_intern_atom_cookie_t proto_cookie = xcb_intern_atom(xcb->connection, 1, 12, "WM_PROTOCOLS");
		xcb_intern_atom_cookie_t del_cookie   = xcb_intern_atom(xcb->connection, 0, 16, "WM_DELETE_WINDOW");
		xcb_intern_atom_reply_t* proto_reply  = xcb_intern_atom_reply(xcb->connection, proto_cookie, nullptr);
		xcb_intern_atom_reply_t* del_reply    = xcb_intern_atom_reply(xcb->connection, del_cookie,   nullptr);
		impl->wm_delete = del_reply->atom;
		xcb_change_property(xcb->connection, XCB_PROP_MODE_REPLACE, xcb->window,
			proto_reply->atom, 4, 32, 1, &del_reply->atom);
		free(proto_reply); free(del_reply);

		xcb_map_window(xcb->connection, xcb->window);
		xcb_flush(xcb->connection);
	}

	Window::~Window()
	{
		WindowImpl* impl = (WindowImpl*)window_delegate;
		XcbWindowHandle* xcb = (XcbWindowHandle*)window_handle;
		if (xcb->connection)
		{
			xcb_destroy_window(xcb->connection, xcb->window);
			xcb_disconnect(xcb->connection);
		}
		delete impl;
		delete xcb;
	}

	Bool Window::Loop()
	{
		WindowImpl* impl = (WindowImpl*)window_delegate;
		XcbWindowHandle* xcb = (XcbWindowHandle*)window_handle;

		xcb_generic_event_t* event = nullptr;
		while ((event = xcb_poll_for_event(xcb->connection)) != nullptr)
		{
			Uint8 type = event->response_type & ~0x80;
			switch (type)
			{
			case XCB_CONFIGURE_NOTIFY:
			{
				xcb_configure_notify_event_t* e = (xcb_configure_notify_event_t*)event;
				if (e->width != impl->width || e->height != impl->height)
				{
					impl->width  = e->width;
					impl->height = e->height;
					WindowEventInfo info{};
					info.handle = window_handle;
					info.width  = (Float)e->width;
					info.height = (Float)e->height;
					window_event.Broadcast(info);
				}
				break;
			}
			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t* e = (xcb_client_message_event_t*)event;
				if (e->data.data32[0] == impl->wm_delete)
				{
					impl->should_quit = true;
				}
				break;
			}
			default:
			{
				WindowEventInfo info{};
				info.handle = window_handle;
				info.msg    = type;
				info.lparam = (Int64)(Uintptr)event;
				window_event.Broadcast(info);
				break;
			}
			}
			free(event);
		}
		return !impl->should_quit;
	}

	void Window::Quit(Int32 exit_code)
	{
		WindowImpl* impl = (WindowImpl*)window_delegate;
		impl->should_quit = true;
	}

	Uint32 Window::Width() const
	{
		return ((WindowImpl*)window_delegate)->width;
	}

	Uint32 Window::Height() const
	{
		return ((WindowImpl*)window_delegate)->height;
	}

	Uint32 Window::PositionX() const { return 0; }
	Uint32 Window::PositionY() const { return 0; }

	void* Window::Handle() const { return window_handle; }

	Bool Window::IsActive() const { return true; }

	void Window::BroadcastEvent(WindowEventInfo const& info)
	{
		window_event.Broadcast(info);
	}
}
