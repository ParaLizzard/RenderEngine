#include "System/Input/Backends/InputBackendWin32.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/MouseEvents.h"
#include "Core/Log.h"

namespace Engine {
    bool InputBackendWin32::Initialize(HWND hwnd) {
        return RegisterMouseDevice(hwnd);
    }

    void InputBackendWin32::Shutdown() {}

    bool InputBackendWin32::RegisterMouseDevice(HWND hwnd) {
        RAWINPUTDEVICE rid;
        rid.usUsagePage = 0x01;
        rid.usUsage     = 0x02;
        rid.dwFlags     = RIDEV_INPUTSINK;
        rid.hwndTarget  = hwnd;

        if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
            LOG_ERROR("Input", "Failed to register Win32 RawInput mouse device!");
            return false;
        }

        LOG_INFO("Input", "Win32 RawInput mouse registered successfully");
        return true;
    }

    bool InputBackendWin32::ProcessRawInput(HRAWINPUT hRawInput, EventDispatcher& dispatcher) {
        UINT size = 0;
        GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        if (size == 0) return false;

        alignas(RAWINPUT) uint8_t buffer[sizeof(RAWINPUT)];
        if (GetRawInputData(hRawInput, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) != size) {
            return false;
        }

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            float dx = static_cast<float>(raw->data.mouse.lLastX);
            float dy = static_cast<float>(raw->data.mouse.lLastY);

            if (dx != 0.0f || dy != 0.0f) {
                MouseRawDeltaEvent event(dx, dy);
                dispatcher.PostEvent(event);
            }
            return true;
        }

        return false;
    }
}