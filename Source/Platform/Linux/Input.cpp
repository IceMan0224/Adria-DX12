#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include "Platform/Input.h"
#include "Platform/Window.h"

namespace adria
{
	static KeyCode XcbKeyToKeyCode(xcb_keysym_t ks)
	{
		if (ks >= 'a' && ks <= 'z') { return (KeyCode)((Int32)KeyCode::A + (ks - 'a')); }
		if (ks >= 'A' && ks <= 'Z') { return (KeyCode)((Int32)KeyCode::A + (ks - 'A')); }
		if (ks >= '0' && ks <= '9') { return (KeyCode)((Int32)KeyCode::Zero + (ks - '0')); }
		switch (ks)
		{
		case 0xff1b: return KeyCode::Escape;
		case 0xff0d: return KeyCode::Enter;
		case 0x20:   return KeyCode::Space;
		case 0xff51: return KeyCode::Left;
		case 0xff52: return KeyCode::Up;
		case 0xff53: return KeyCode::Right;
		case 0xff54: return KeyCode::Down;
		default:     return KeyCode::Unknown;
		}
	}

	void Input::Initialize(Window* window)
	{
		window->GetWindowEvent().AddLambda([this](WindowEventInfo const& info)
		{
			xcb_generic_event_t* raw = (xcb_generic_event_t*)(uintptr_t)info.lparam;
			if (!raw) { return; }

			Uint8 type = info.msg;
			switch (type)
			{
			case XCB_KEY_PRESS:
			case XCB_KEY_RELEASE:
			{
				xcb_key_press_event_t* e = (xcb_key_press_event_t*)raw;
				xcb_connection_t* conn = (xcb_connection_t*)((struct { xcb_connection_t* c; Uint32 w; }*)info.handle)->c;
				xcb_key_symbols_t* syms = xcb_key_symbols_alloc(conn);
				xcb_keysym_t ks = xcb_key_symbols_get_keysym(syms, e->detail, 0);
				xcb_key_symbols_free(syms);
				KeyCode kc = XcbKeyToKeyCode(ks);
				if (kc != KeyCode::Unknown)
				{
					keys[(Uint32)kc] = (type == XCB_KEY_PRESS);
				}
				break;
			}
			case XCB_BUTTON_PRESS:
			case XCB_BUTTON_RELEASE:
			{
				xcb_button_press_event_t* e = (xcb_button_press_event_t*)raw;
				Bool pressed = (type == XCB_BUTTON_PRESS);
				if (e->detail == 1) { mouse_buttons[(Uint32)MouseButton::Left]   = pressed; }
				if (e->detail == 3) { mouse_buttons[(Uint32)MouseButton::Right]  = pressed; }
				if (e->detail == 2) { mouse_buttons[(Uint32)MouseButton::Middle] = pressed; }
				break;
			}
			case XCB_MOTION_NOTIFY:
			{
				xcb_motion_notify_event_t* e = (xcb_motion_notify_event_t*)raw;
				mouse_position_x = e->event_x;
				mouse_position_y = e->event_y;
				break;
			}
			default: break;
			}
		});
	}
}
