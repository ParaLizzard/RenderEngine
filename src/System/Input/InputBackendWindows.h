#pragma once
#include "System/Input/InputBackend.h"
#include <windows.h>

namespace Engine {

    class InputBackendWindows : public InputBackend
    {
    private:
        HWND hwnd;
        HRAWINPUT rawInputDevices[2]; // Mouse, Keyboard
    public:
        void Initialize(Engine::Window & window) override;
        void Shutdown() override;

        const std::queue<InputEvent> &GetEventQueue() const override;
        void ClearEventQueue() override;

        void ProcessEvents(LPARAM param) override;
        KeyCode MapWinKeyToEngineKey(USHORT vkey);

        void PollGamepads() override;


        void SetMouseCapture(bool captured) override;
        bool IsMouseCaptured() const override;
        void SetMouseVisible(bool visible) override;
        glm::ivec2 GetMouseAbsolutePosition() const override;
    private:
        POINT lastMousePos = {0,0};
        std::queue<InputEvent> eventQueue;
    };

} // namespace Engine