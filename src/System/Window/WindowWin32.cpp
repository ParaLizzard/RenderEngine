#define VK_USE_PLATFORM_WIN32_KHR

#include "System/Window/WindowWin32.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/WindowEvents.h"
#include "System/Events/KeyEvents.h"
#include "System/Events/MouseEvents.h"
#include <windowsx.h>
#include <dwmapi.h>
#include "System/Input/Backends/InputBackendWin32.h"

namespace Engine {

    WindowWin32::WindowWin32(const WindowProps& props)
        : properties(props), hInstance(GetModuleHandle(nullptr))
    {
        if (HMODULE user32 = GetModuleHandleA("user32.dll")) {
            using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(void*);
            if (auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
                setContext(reinterpret_cast<void*>(-4));
            }
        }

        const char* className = "RenderEngine_WindowClass";
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = className;
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassEx(&wc);



        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!properties.resizable) {
            style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;
        }

        RECT wr{ 0, 0, static_cast<LONG>(properties.width), static_cast<LONG>(properties.height) };
        AdjustWindowRectEx(&wr, style, FALSE, 0);

        int windowWidth = wr.right - wr.left;
        int windowHeight = wr.bottom - wr.top;

        hwnd = CreateWindowEx(
            0,
            className,
            properties.title.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowWidth, windowHeight,
            nullptr, nullptr, hInstance,
            this
        );

        InputBackendWin32::Initialize(hwnd);

        DragAcceptFiles(hwnd, TRUE);

        ENGINE_ASSERT(hwnd != nullptr, "Failed to create Win32 window! GetLastError: {}", GetLastError());

        ApplyDpiScaling();
        EnableDarkMode();

        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);
    }

    WindowWin32::~WindowWin32()
    {
        InputBackendWin32::Shutdown();
        DestroyWindow(hwnd);
    }

    static KeyCode MapWin32KeyToKeyCode(WPARAM wParam) {
        if (wParam >= '0' && wParam <= '9') {
            return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::D0) + (wParam - '0'));
        }
        if (wParam >= 'A' && wParam <= 'Z') {
            return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::A) + (wParam - 'A'));
        }
        if (wParam >= VK_F1 && wParam <= VK_F12) {
            return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::F1) + (wParam - VK_F1));
        }
        if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
            return static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::KP0) + (wParam - VK_NUMPAD0));
        }

        switch (wParam) {
            case VK_SPACE:      return KeyCode::Space;
            case VK_OEM_7:      return KeyCode::Apostrophe;
            case VK_OEM_COMMA:  return KeyCode::Comma;
            case VK_OEM_MINUS:  return KeyCode::Minus;
            case VK_OEM_PERIOD: return KeyCode::Period;
            case VK_OEM_2:      return KeyCode::Slash;
            case VK_OEM_1:      return KeyCode::Semicolon;
            case VK_OEM_PLUS:   return KeyCode::Equal;
            case VK_OEM_4:      return KeyCode::LeftBracket;
            case VK_OEM_5:      return KeyCode::Backslash;
            case VK_OEM_6:      return KeyCode::RightBracket;
            case VK_OEM_3:      return KeyCode::GraveAccent;
            case VK_ESCAPE:     return KeyCode::Escape;
            case VK_RETURN:     return KeyCode::Enter;
            case VK_TAB:        return KeyCode::Tab;
            case VK_BACK:       return KeyCode::Backspace;
            case VK_INSERT:     return KeyCode::Insert;
            case VK_DELETE:     return KeyCode::Delete;
            case VK_RIGHT:      return KeyCode::Right;
            case VK_LEFT:       return KeyCode::Left;
            case VK_DOWN:       return KeyCode::Down;
            case VK_UP:         return KeyCode::Up;
            case VK_PRIOR:      return KeyCode::PageUp;
            case VK_NEXT:       return KeyCode::PageDown;
            case VK_HOME:       return KeyCode::Home;
            case VK_END:        return KeyCode::End;
            case VK_CAPITAL:    return KeyCode::CapsLock;
            case VK_SCROLL:     return KeyCode::ScrollLock;
            case VK_NUMLOCK:    return KeyCode::NumLock;
            case VK_SNAPSHOT:   return KeyCode::PrintScreen;
            case VK_PAUSE:      return KeyCode::Pause;
            case VK_SHIFT:
            case VK_LSHIFT:     return KeyCode::LeftShift;
            case VK_RSHIFT:     return KeyCode::RightShift;
            case VK_CONTROL:
            case VK_LCONTROL:   return KeyCode::LeftControl;
            case VK_RCONTROL:   return KeyCode::RightControl;
            case VK_MENU:
            case VK_LMENU:      return KeyCode::LeftAlt;
            case VK_RMENU:      return KeyCode::RightAlt;
            case VK_LWIN:       return KeyCode::LeftSuper;
            case VK_RWIN:       return KeyCode::RightSuper;
            default:            return KeyCode::Unknown;
        }
    }

    LRESULT WindowWin32::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
            case WM_CLOSE: {
                shouldClose = true;
                WindowCloseEvent event;
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_SIZE: {
                    uint32_t width = LOWORD(lParam);
                    uint32_t height = HIWORD(lParam);
                    if (wParam == SIZE_MINIMIZED) {
                        properties.width = 0;
                        properties.height = 0;
                        WindowIconifyEvent iconifyEvent(true);
                        EventDispatcher::Get().PostEvent(iconifyEvent);
                    } else {
                        if (properties.width == 0 && properties.height == 0) {
                            WindowIconifyEvent iconifyEvent(false);
                            EventDispatcher::Get().PostEvent(iconifyEvent);
                        }
                        properties.width = width;
                        properties.height = height;
                    }
                    WindowResizeEvent event(properties.width, properties.height);
                    EventDispatcher::Get().PostEvent(event);
                    return 0;
            }
            case WM_SETFOCUS: {
                    WindowFocusEvent event(true);
                    EventDispatcher::Get().PostEvent(event);
                    return 0;
            }
            case WM_KILLFOCUS: {
                    WindowFocusEvent event(false);
                    EventDispatcher::Get().PostEvent(event);
                    return 0;
            }
            case WM_DPICHANGED: {
                UINT dpiX = LOWORD(wParam);
                UINT dpiY = HIWORD(wParam);
                dpiScale = Vec2(
                    static_cast<float>(dpiX) / 96.0f,
                    static_cast<float>(dpiY) / 96.0f
                    );

                auto *prcNewWindow = reinterpret_cast<RECT *>(lParam);
                SetWindowPos(hwnd,
                             nullptr,
                             prcNewWindow->left,
                             prcNewWindow->top,
                             prcNewWindow->right - prcNewWindow->left,
                             prcNewWindow->bottom - prcNewWindow->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            case WM_DROPFILES: {
                    HDROP hDrop = reinterpret_cast<HDROP>(wParam);
                    UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);
                    std::vector<std::string> paths;
                    paths.reserve(fileCount);
                    char filename[MAX_PATH];
                    for (UINT i = 0; i < fileCount; ++i) {
                        if (DragQueryFileA(hDrop, i, filename, MAX_PATH)) {
                            paths.emplace_back(filename);
                        }
                    }
                    DragFinish(hDrop);
                    WindowDropFilesEvent event(std::move(paths));
                    EventDispatcher::Get().PostEvent(event);
                    return 0;
            }

            case WM_INPUT: {
                    HRAWINPUT hRawInput = reinterpret_cast<HRAWINPUT>(lParam);
                    if (InputBackendWin32::ProcessRawInput(hRawInput, EventDispatcher::Get())) {
                        return 0;
                    }
                    break;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                KeyCode key = MapWin32KeyToKeyCode(wParam);
                if (key != KeyCode::Unknown) {
                    bool isRepeat = (lParam & (1 << 30)) != 0;
                    KeyPressedEvent event(key, isRepeat);
                    EventDispatcher::Get().PostEvent(event);
                }
                return 0;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                KeyCode key = MapWin32KeyToKeyCode(wParam);
                if (key != KeyCode::Unknown) {
                    KeyReleasedEvent event(key);
                    EventDispatcher::Get().PostEvent(event);
                }
                return 0;
            }
            case WM_CHAR: {
                if (wParam > 0 && wParam < 0x10000) {
                    KeyTypedEvent event(static_cast<uint32_t>(wParam));
                    EventDispatcher::Get().PostEvent(event);
                }
                return 0;
            }

            case WM_MOUSEMOVE: {
                    if (!mouseHovering) {
                        mouseHovering = true;
                        TRACKMOUSEEVENT tme{};
                        tme.cbSize = sizeof(TRACKMOUSEEVENT);
                        tme.dwFlags = TME_LEAVE;
                        tme.hwndTrack = hwnd;
                        TrackMouseEvent(&tme);
                        MouseEnterEvent enterEvent(true);
                        EventDispatcher::Get().PostEvent(enterEvent);
                    }
                    float x = static_cast<float>(GET_X_LPARAM(lParam));
                    float y = static_cast<float>(GET_Y_LPARAM(lParam));
                    MouseMovedEvent event(x, y);
                    EventDispatcher::Get().PostEvent(event);
                    return 0;
            }

            case WM_MOUSELEAVE: {
                    mouseHovering = false;
                    MouseEnterEvent leaveEvent(false);
                    EventDispatcher::Get().PostEvent(leaveEvent);
                    return 0;
            }

            case WM_LBUTTONDOWN: {
                SetCapture(hwnd);
                MouseButtonPressedEvent event(MouseButton::Left);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_LBUTTONUP: {
                ReleaseCapture();
                MouseButtonReleasedEvent event(MouseButton::Left);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_RBUTTONDOWN: {
                SetCapture(hwnd);
                MouseButtonPressedEvent event(MouseButton::Right);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_RBUTTONUP: {
                ReleaseCapture();
                MouseButtonReleasedEvent event(MouseButton::Right);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_MBUTTONDOWN: {
                SetCapture(hwnd);
                MouseButtonPressedEvent event(MouseButton::Middle);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_MBUTTONUP: {
                ReleaseCapture();
                MouseButtonReleasedEvent event(MouseButton::Middle);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_XBUTTONDOWN: {
                SetCapture(hwnd);
                MouseButton btn = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::Button4 : MouseButton::Button5;
                MouseButtonPressedEvent event(btn);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_XBUTTONUP: {
                ReleaseCapture();
                MouseButton btn = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::Button4 : MouseButton::Button5;
                MouseButtonReleasedEvent event(btn);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }

            case WM_MOUSEWHEEL: {
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
                MouseScrolledEvent event(0.0f, delta);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
            case WM_MOUSEHWHEEL: {
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
                MouseScrolledEvent event(delta, 0.0f);
                EventDispatcher::Get().PostEvent(event);
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void WindowWin32::SetCursorMode(CursorMode mode)
    {
        cursorMode = mode;
        switch (mode) {
        case CursorMode::Normal: {
            while (ShowCursor(TRUE) < 0);
            ClipCursor(nullptr);
            break;
        }
        case CursorMode::Hidden: {
            while (ShowCursor(FALSE) >= 0);
            ClipCursor(nullptr);
            break;
        }
        case CursorMode::Disabled: {
            while (ShowCursor(FALSE) >= 0);
            RECT rect;
            GetClientRect(hwnd, &rect);
            MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
            break;
        }
        }
    }

    bool WindowWin32::CreateVulkanSurface(VkInstance instance, VkSurfaceKHR*outSurface)
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = hwnd;
        createInfo.hinstance = hInstance;

        VkResult result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, outSurface);
        if (result != VK_SUCCESS) {
            LOG_FATAL("Vulkan", "Failed to create Win32 Vulkan surface! Error: {}", (int)result);
            return false;
        }
        return true;
    }

    LRESULT WindowWin32::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_NCCREATE) {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto* pWnd = static_cast<WindowWin32*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
            pWnd->hwnd = hwnd;
            return pWnd->HandleMessage(uMsg, wParam, lParam);
        }

        auto* pWnd = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (pWnd) {
            return pWnd->HandleMessage(uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    void WindowWin32::ApplyDpiScaling()
    {
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (HMODULE user32 = GetModuleHandleA("user32.dll")) {
            using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
            auto getDpi = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
            if (getDpi && hwnd) {
                UINT dpi = getDpi(hwnd);
                dpiX = dpi;
                dpiY = dpi;
            } else if (HDC hdc = GetDC(hwnd)) {
                dpiX = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
                dpiY = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSY));
                ReleaseDC(hwnd, hdc);
            }
        }
        dpiScale = Vec2(
            static_cast<float>(dpiX) / 96.0f,
            static_cast<float>(dpiY) / 96.0f
        );
    }

    void WindowWin32::EnableDarkMode()
    {
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    }

    void WindowWin32::PollEvents()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                shouldClose = true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void WindowWin32::SetTitle(std::string_view title)
    {
        properties.title = title;
        if (hwnd) {
            SetWindowTextA(hwnd, properties.title.c_str());
        }
    }
}