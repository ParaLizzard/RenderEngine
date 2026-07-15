#pragma once
#include <memory>
#include <queue>
#include <glm/vec2.hpp>
#include "System/Input/InputTypes.h"

namespace Engine {
    class Window;

    class InputBackend
    {
    public:
        virtual ~InputBackend() = default;

        virtual void Initialize(Engine::Window& window) = 0;
        virtual void Shutdown() = 0;

        virtual const std::queue<InputEvent>& GetEventQueue() const = 0;
        virtual void ClearEventQueue() = 0;

        virtual void ProcessEvents(intptr_t param) = 0;

        virtual void PollGamepads() = 0;

        virtual void SetMouseCapture(bool captured) = 0;
        [[nodiscard]] virtual bool IsMouseCaptured() const = 0;
        virtual void SetMouseVisible(bool visible) = 0;
        [[nodiscard]] virtual glm::ivec2 GetMouseAbsolutePosition() const = 0;
    };
}