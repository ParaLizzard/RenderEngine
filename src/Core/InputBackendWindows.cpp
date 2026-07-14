//
// Created by Jan Varga on 13.07.2026.
//

#include "InputBackendWindows.h"

#include "GLFW/glfw3.h"

void InputBackendWindows::Initialize(void *windowHandle)
{
    hwnd = static_cast<HWND>(windowHandle);

    RAWINPUTDEVICE rid[3];

    // Keyboard
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = hwnd;

    // Mouse
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = hwnd;

    // Game Pad
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x05;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = hwnd;

    if (!RegisterRawInputDevices(rid, 3, sizeof(rid[0]))) {
        //errors
    }
}

void InputBackendWindows::Shutdown()
{}

const std::queue<InputEvent> & InputBackendWindows::GetEventQueue() const
{}

void InputBackendWindows::ClearEventQueue()
{}

void InputBackendWindows::ProcessEvents(LPARAM param, InputManager& manager)
{
    uint32_t dwSize = 0;
    GetRawInputData((HRAWINPUT)param, RID_INPUT, nullptr, &dwSize,
                       sizeof(RAWINPUTHEADER));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<a*>(glfwGetWindowUserPointer(hwnd));

    if (msg == WM_INPUT) {
        app->rawInputBackend.ProcessRawInput(lParam, &app->inputManager);
    }
    // ... other messages
}

void InputBackendWindows::SetMouseCapture(bool captured)
{}

bool InputBackendWindows::IsMouseCaptured() const
{}

void InputBackendWindows::SetMouseVisible(bool visible)
{}

glm::ivec2 InputBackendWindows::GetMouseAbsolutePosition() const
{}
