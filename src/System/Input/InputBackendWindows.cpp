//
// Created by Jan Varga on 13.07.2026.
//

#include "System/Input/InputBackendWindows.h"
#include "System/Window/Window.h"

namespace Engine {

void InputBackendWindows::Initialize(Engine::Window & window)
{
    hwnd = static_cast<HWND>(window.getWindowHandle());

    RAWINPUTDEVICE rid[2];

    // Keyboard
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;

    // Mouse
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x02;
    rid[1].dwFlags = 0;
    rid[1].hwndTarget = hwnd;

    if (!RegisterRawInputDevices(rid, 2, sizeof(rid[0]))) {
        //errors
    }

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    lastMousePos = {pt.x, pt.y};
}

void InputBackendWindows::Shutdown()
{}

const std::queue<InputEvent> & InputBackendWindows::GetEventQueue() const
{
    return eventQueue;
}

void InputBackendWindows::ClearEventQueue()
{
    eventQueue = std::queue<InputEvent>();
}

void InputBackendWindows::ProcessEvents(LPARAM param)
{
    uint32_t dwSize = 0;
    GetRawInputData((HRAWINPUT)param, RID_INPUT, nullptr, &dwSize,
                       sizeof(RAWINPUTHEADER));

    if (dwSize == 0) return;

    auto lpb = std::make_unique<BYTE[]>(dwSize);

    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(param), RID_INPUT, lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
        return;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.get());

    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        USHORT vkey = raw->data.keyboard.VKey;
        USHORT flags = raw->data.keyboard.Flags;

        if (vkey == 255) return;

        KeyCode engineKey = MapWinKeyToEngineKey(vkey);
        if (engineKey == KeyCode::Unknown) return;

        InputEvent event{};
        event.timestamp = 0;

        if (flags & RI_KEY_BREAK) {
            event.type = InputEventType::KeyUp;
            event.data.keyboard.key = engineKey;
            eventQueue.push(event);
        } else {
            event.type = InputEventType::KeyDown;
            event.data.keyboard.key = engineKey;
            eventQueue.push(event);
        }
    }else if (raw->header.dwType == RIM_TYPEMOUSE) {
        USHORT flags = raw->data.mouse.usFlags;
        LONG deltaX = raw->data.mouse.lLastX;
        LONG deltaY = raw->data.mouse.lLastY;
        USHORT buttonFlags = raw->data.mouse.usButtonFlags;

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);

        if (deltaX != 0 || deltaY != 0) {
            InputEvent event{};
            event.type = InputEventType::MouseMove;
            event.data.mouseMotion.x = pt.x;
            event.data.mouseMotion.y = pt.y;
            event.data.mouseMotion.deltaX = static_cast<int32_t>(deltaX);
            event.data.mouseMotion.deltaY = static_cast<int32_t>(deltaY);
            eventQueue.push(event);
        }

        auto pushButtonEvent = [&](MouseButton button, bool isDown) {
            InputEvent event{};
            event.type = isDown ? InputEventType::MouseButtonDown : InputEventType::MouseButtonUp;
            event.data.mouseButton.button = button;
            event.data.mouseButton.x = pt.x;
            event.data.mouseButton.y = pt.y;
            eventQueue.push(event);
        };

        if (buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)  pushButtonEvent(MouseButton::Left, true);
        if (buttonFlags & RI_MOUSE_LEFT_BUTTON_UP)    pushButtonEvent(MouseButton::Left, false);

        if (buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) pushButtonEvent(MouseButton::Right, true);
        if (buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP)   pushButtonEvent(MouseButton::Right, false);

        if (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) pushButtonEvent(MouseButton::Middle, true);
        if (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)   pushButtonEvent(MouseButton::Middle, false);

        if (buttonFlags & RI_MOUSE_WHEEL) {
            short rawWheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
            float wheelScroll = static_cast<float>(rawWheelDelta) / WHEEL_DELTA;

            InputEvent event{};
            event.type = InputEventType::MouseScroll;
            event.data.mouseScroll.delta = wheelScroll;
            eventQueue.push(event);
        }
    }
}

// Not all, needs update
KeyCode InputBackendWindows::MapWinKeyToEngineKey(USHORT vkey) {
    switch (vkey) {
    case 'A': return KeyCode::A;
    case 'B': return KeyCode::B;
    case 'C': return KeyCode::C;
    case 'D': return KeyCode::D;
    case 'E': return KeyCode::E;
    case 'F': return KeyCode::F;
    case 'G': return KeyCode::G;
    case 'H': return KeyCode::H;
    case 'I': return KeyCode::I;
    case 'J': return KeyCode::J;
    case 'K': return KeyCode::K;
    case 'L': return KeyCode::L;
    case 'M': return KeyCode::M;
    case 'N': return KeyCode::N;
    case 'O': return KeyCode::O;
    case 'P': return KeyCode::P;
    case 'Q': return KeyCode::Q;
    case 'R': return KeyCode::R;
    case 'S': return KeyCode::S;
    case 'T': return KeyCode::T;
    case 'U': return KeyCode::U;
    case 'V': return KeyCode::V;
    case 'W': return KeyCode::W;
    case 'X': return KeyCode::X;
    case 'Y': return KeyCode::Y;
    case 'Z': return KeyCode::Z;

    case VK_F1: return KeyCode::F1;
    case VK_F2: return KeyCode::F2;
    case VK_F3: return KeyCode::F3;
    case VK_F4: return KeyCode::F4;
    case VK_F5: return KeyCode::F5;
    case VK_F6: return KeyCode::F6;
    case VK_F7: return KeyCode::F7;
    case VK_F8: return KeyCode::F8;
    case VK_F9: return KeyCode::F9;
    case VK_F10: return KeyCode::F10;
    case VK_F11: return KeyCode::F11;
    case VK_F12: return KeyCode::F12;

    case VK_SPACE:   return KeyCode::Space;
    case VK_ESCAPE:  return KeyCode::Escape;
    case VK_RETURN:  return KeyCode::Enter;
    case VK_TAB:     return KeyCode::Tab;

    case VK_LEFT:    return KeyCode::Left;
    case VK_RIGHT:   return KeyCode::Right;
    case VK_UP:      return KeyCode::Up;
    case VK_DOWN:    return KeyCode::Down;

    case VK_LSHIFT:  return KeyCode::LeftShift;
    case VK_RSHIFT:  return KeyCode::RightShift;
    case VK_LCONTROL:return KeyCode::LeftCtrl;
    case VK_RCONTROL:return KeyCode::RightCtrl;

    default: return KeyCode::Unknown;
    }
}

void InputBackendWindows::SetMouseCapture(bool captured)
{
    if (captured) {
        SetCapture(hwnd);

        RECT rect;
        if (GetClientRect(hwnd, &rect)) {
            POINT topLeft = { rect.left, rect.top };
            POINT bottomRight = { rect.right, rect.bottom };

            ClientToScreen(hwnd, &topLeft);
            ClientToScreen(hwnd, &bottomRight);

            RECT clipRect = { topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
            ClipCursor(&clipRect);
        }  else {
            ClipCursor(nullptr);
            ReleaseCapture();
        }
    }
}

bool InputBackendWindows::IsMouseCaptured() const
{
    return GetCapture() == hwnd;
}

void InputBackendWindows::SetMouseVisible(bool visible)
{
    ShowCursor(visible);
}

glm::ivec2 InputBackendWindows::GetMouseAbsolutePosition() const
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    return {pt.x, pt.y};
}

void InputBackendWindows::PollGamepads()
{
    // Stub
}

} // namespace Engine