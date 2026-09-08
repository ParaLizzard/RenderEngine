#pragma once
#include <windows.h>
#include <cstdint>

namespace Engine {
    class EventDispatcher;

    class InputBackendWin32 {
    public:
        static bool Initialize(HWND hwnd);
        static void Shutdown();

        static bool ProcessRawInput(HRAWINPUT hRawInput, EventDispatcher& dispatcher);

    private:
        static bool RegisterMouseDevice(HWND hwnd);
    };
}