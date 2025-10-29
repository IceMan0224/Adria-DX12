#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include "Platform/Input.h"
#include "Platform/Window.h"

namespace adria
{
    static KeyCode MapNSKeyCode(unsigned short keyCode)
    {
        using enum KeyCode;

        switch (keyCode)
        {
            case kVK_F1: return F1;
            case kVK_F2: return F2;
            case kVK_F3: return F3;
            case kVK_F4: return F4;
            case kVK_F5: return F5;
            case kVK_F6: return F6;
            case kVK_F7: return F7;
            case kVK_F8: return F8;
            case kVK_F9: return F9;
            case kVK_F10: return F10;
            case kVK_F11: return F11;
            case kVK_F12: return F12;

            case kVK_ANSI_0: return Alpha0;
            case kVK_ANSI_1: return Alpha1;
            case kVK_ANSI_2: return Alpha2;
            case kVK_ANSI_3: return Alpha3;
            case kVK_ANSI_4: return Alpha4;
            case kVK_ANSI_5: return Alpha5;
            case kVK_ANSI_6: return Alpha6;
            case kVK_ANSI_7: return Alpha7;
            case kVK_ANSI_8: return Alpha8;
            case kVK_ANSI_9: return Alpha9;

            case kVK_ANSI_Keypad0: return Numpad0;
            case kVK_ANSI_Keypad1: return Numpad1;
            case kVK_ANSI_Keypad2: return Numpad2;
            case kVK_ANSI_Keypad3: return Numpad3;
            case kVK_ANSI_Keypad4: return Numpad4;
            case kVK_ANSI_Keypad5: return Numpad5;
            case kVK_ANSI_Keypad6: return Numpad6;
            case kVK_ANSI_Keypad7: return Numpad7;
            case kVK_ANSI_Keypad8: return Numpad8;
            case kVK_ANSI_Keypad9: return Numpad9;

            case kVK_ANSI_Q: return Q;
            case kVK_ANSI_W: return W;
            case kVK_ANSI_E: return E;
            case kVK_ANSI_R: return R;
            case kVK_ANSI_T: return T;
            case kVK_ANSI_Y: return Y;
            case kVK_ANSI_U: return U;
            case kVK_ANSI_I: return I;
            case kVK_ANSI_O: return O;
            case kVK_ANSI_P: return P;
            case kVK_ANSI_A: return A;
            case kVK_ANSI_S: return S;
            case kVK_ANSI_D: return D;
            case kVK_ANSI_F: return F;
            case kVK_ANSI_G: return G;
            case kVK_ANSI_H: return H;
            case kVK_ANSI_J: return J;
            case kVK_ANSI_K: return K;
            case kVK_ANSI_L: return L;
            case kVK_ANSI_Z: return Z;
            case kVK_ANSI_X: return X;
            case kVK_ANSI_C: return C;
            case kVK_ANSI_V: return V;
            case kVK_ANSI_B: return B;
            case kVK_ANSI_N: return N;
            case kVK_ANSI_M: return M;

            case kVK_Escape: return Esc;
            case kVK_Tab: return Tab;
            case kVK_Shift: return ShiftLeft;
            case kVK_RightShift: return ShiftRight;
            case kVK_Control: return CtrlLeft;
            case kVK_RightControl: return CtrlRight;
            case kVK_Option: return AltLeft;
            case kVK_RightOption: return AltRight;
            case kVK_Space: return Space;
            case kVK_CapsLock: return CapsLock;
            case kVK_Delete: return Backspace;
            case kVK_Return: return Enter;
            case kVK_ForwardDelete: return Delete;
            case kVK_LeftArrow: return ArrowLeft;
            case kVK_RightArrow: return ArrowRight;
            case kVK_UpArrow: return ArrowUp;
            case kVK_DownArrow: return ArrowDown;
            case kVK_PageUp: return PageUp;
            case kVK_PageDown: return PageDown;
            case kVK_Home: return Home;
            case kVK_End: return End;
            case kVK_ANSI_Grave: return Tilde;

            default: return Count;
        }
    }

    Input::Input() : keys{}, prev_keys{}, input_events{}
    {
        @autoreleasepool
        {
            NSPoint mouseLocation = [NSEvent mouseLocation];
            mouse_position_x = static_cast<Float>(mouseLocation.x);
            mouse_position_y = static_cast<Float>(mouseLocation.y);
        }
    }

    void Input::Tick()
    {
        @autoreleasepool
        {
            prev_keys = std::move(keys);

            prev_mouse_position_x = mouse_position_x;
            prev_mouse_position_y = mouse_position_y;

            if (window->IsActive())
            {
                NSPoint mouseLocation = [NSEvent mouseLocation];
                mouse_position_x = static_cast<Float>(mouseLocation.x);
                mouse_position_y = static_cast<Float>(mouseLocation.y);

                using enum KeyCode;

                keys[(Uint64)MouseLeft] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft);
                keys[(Uint64)MouseMiddle] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonCenter);
                keys[(Uint64)MouseRight] = CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight);

                keys[(Uint64)F1] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F1);
                keys[(Uint64)F2] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F2);
                keys[(Uint64)F3] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F3);
                keys[(Uint64)F4] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F4);
                keys[(Uint64)F5] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F5);
                keys[(Uint64)F6] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F6);
                keys[(Uint64)F7] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F7);
                keys[(Uint64)F8] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F8);
                keys[(Uint64)F9] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F9);
                keys[(Uint64)F10] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F10);
                keys[(Uint64)F11] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F11);
                keys[(Uint64)F12] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_F12);

                keys[(Uint64)Alpha0] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_0);
                keys[(Uint64)Alpha1] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_1);
                keys[(Uint64)Alpha2] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_2);
                keys[(Uint64)Alpha3] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_3);
                keys[(Uint64)Alpha4] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_4);
                keys[(Uint64)Alpha5] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_5);
                keys[(Uint64)Alpha6] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_6);
                keys[(Uint64)Alpha7] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_7);
                keys[(Uint64)Alpha8] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_8);
                keys[(Uint64)Alpha9] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_9);

                keys[(Uint64)Numpad0] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad0);
                keys[(Uint64)Numpad1] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad1);
                keys[(Uint64)Numpad2] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad2);
                keys[(Uint64)Numpad3] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad3);
                keys[(Uint64)Numpad4] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad4);
                keys[(Uint64)Numpad5] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad5);
                keys[(Uint64)Numpad6] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad6);
                keys[(Uint64)Numpad7] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad7);
                keys[(Uint64)Numpad8] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad8);
                keys[(Uint64)Numpad9] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Keypad9);

                keys[(Uint64)Q] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Q);
                keys[(Uint64)W] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_W);
                keys[(Uint64)E] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_E);
                keys[(Uint64)R] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_R);
                keys[(Uint64)T] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_T);
                keys[(Uint64)Y] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Y);
                keys[(Uint64)U] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_U);
                keys[(Uint64)I] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_I);
                keys[(Uint64)O] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_O);
                keys[(Uint64)P] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_P);
                keys[(Uint64)A] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_A);
                keys[(Uint64)S] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_S);
                keys[(Uint64)D] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_D);
                keys[(Uint64)F] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_F);
                keys[(Uint64)G] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_G);
                keys[(Uint64)H] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_H);
                keys[(Uint64)J] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_J);
                keys[(Uint64)K] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_K);
                keys[(Uint64)L] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_L);
                keys[(Uint64)Z] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Z);
                keys[(Uint64)X] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_X);
                keys[(Uint64)C] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_C);
                keys[(Uint64)V] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_V);
                keys[(Uint64)B] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_B);
                keys[(Uint64)N] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_N);
                keys[(Uint64)M] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_M);

                keys[(Uint64)Esc] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Escape);
                keys[(Uint64)Tab] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Tab);
                keys[(Uint64)ShiftLeft] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Shift);
                keys[(Uint64)ShiftRight] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightShift);
                keys[(Uint64)CtrlLeft] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Control);
                keys[(Uint64)CtrlRight] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightControl);
                keys[(Uint64)AltLeft] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Option);
                keys[(Uint64)AltRight] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightOption);
                keys[(Uint64)Space] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Space);
                keys[(Uint64)CapsLock] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_CapsLock);
                keys[(Uint64)Backspace] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Delete);
                keys[(Uint64)Enter] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Return);
                keys[(Uint64)Delete] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ForwardDelete);
                keys[(Uint64)ArrowLeft] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_LeftArrow);
                keys[(Uint64)ArrowRight] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightArrow);
                keys[(Uint64)ArrowUp] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_UpArrow);
                keys[(Uint64)ArrowDown] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_DownArrow);
                keys[(Uint64)PageUp] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_PageUp);
                keys[(Uint64)PageDown] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_PageDown);
                keys[(Uint64)Home] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Home);
                keys[(Uint64)End] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_End);
                keys[(Uint64)Tilde] = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_ANSI_Grave);

                if (GetKey(KeyCode::Esc))
                {
                    window->Quit(0);
                }
            }
        }
    }

    void Input::OnWindowEvent(WindowEventInfo const& data)
    {
        if (data.width > 0 && data.height > 0)
        {
            if (!resizing)
            {
                input_events.window_resized_event.Broadcast((Uint32)data.width, (Uint32)data.height);
            }
        }
    }

    void Input::SetMouseVisibility(Bool visible)
    {
        @autoreleasepool
        {
            if (visible)
            {
                [NSCursor unhide];
            }
            else
            {
                [NSCursor hide];
            }
        }
    }

    void Input::SetMousePosition(Float xpos, Float ypos)
    {
        @autoreleasepool
        {
            NSWindow* nsWindow = (__bridge NSWindow*)window->Handle();
            if (nsWindow && [nsWindow isKeyWindow])
            {
                NSPoint windowPoint = NSMakePoint(xpos, ypos);
                NSPoint screenPoint = [nsWindow convertPointToScreen:windowPoint];

                CGPoint cgPoint = CGPointMake(screenPoint.x, screenPoint.y);
                CGWarpMouseCursorPosition(cgPoint);
            }
        }
    }
}
