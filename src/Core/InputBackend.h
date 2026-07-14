#pragma once
#include <queue>
#include "InputBackendWindows.h"
#include "Core/InputTypes.h"

class InputManager;

class InputBackend
{
public:
    virtual ~InputBackend() = default;

    virtual void Initialize(void* windowHandle) = 0;
    virtual void Shutdown() = 0;

    virtual const std::queue<InputEvent>& GetEventQueue() const = 0;
    virtual void ClearEventQueue() = 0;

    virtual void ProcessEvents(long long param, InputManager& manager) = 0;

    virtual void PollGamepads() = 0;

    virtual void SetMouseCapture(bool captured) = 0;
    [[nodiscard]] virtual bool IsMouseCaptured() const = 0;
    virtual void SetMouseVisible(bool visible) = 0;
    [[nodiscard]] virtual glm::ivec2 GetMouseAbsolutePosition() const = 0;
};
