#pragma once
#include "InputBackend.h"
#include <windows.h>

class InputBackendWindows : InputBackend
{
private:
    HWND hwnd;
    HRAWINPUT rawInputDevices[2]; // Mouse, Keyboard
public:
    void Initialize(void *windowHandle) override;
    void Shutdown() override;

    const std::queue<InputEvent> &GetEventQueue() const override;
    void ClearEventQueue() override;

    void ProcessEvents(LPARAM param, InputManager &manager) override;

    void PollGamepads() override;

    void SetMouseCapture(bool captured) override;
    bool IsMouseCaptured() const override;
    void SetMouseVisible(bool visible) override;
    glm::ivec2 GetMouseAbsolutePosition() const override;
};
