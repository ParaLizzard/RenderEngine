#pragma once
#include <array>
#include <memory>
#include <unordered_map>

#include <glm/vec2.hpp>

#include "System/Input/InputBackend.h"
#include "System/Input/InputTypes.h"

namespace Engine {
    class Window;

    class InputManager
    {
    private:
        std::unique_ptr<InputBackend> backend;

        std::unordered_map<KeyCode, InputSource> currentFrame;
        std::unordered_map<KeyCode, InputSource> nextFrame;

        std::unordered_map<MouseButton, InputSource> mouseCurrentFrame;
        std::unordered_map<MouseButton, InputSource> mouseNextFrame;
        glm::vec2 mouseDelta{0.0f, 0.0f};
        glm::vec2 accumulatedMouseDelta{0.0f, 0.0f};
        glm::vec2 mousePosition{0.0f, 0.0f};

        struct GamepadState {
            std::unordered_map<GamepadButton, InputSource> buttons;
            std::unordered_map<GamepadAxis, glm::vec2> axes;
            std::unordered_map<GamepadTrigger, float> triggers;
        };
        std::array<GamepadState, 4> gamepads;

    public:
        InputManager(std::unique_ptr<InputBackend> backend_)
            : backend(std::move(backend_)) {}

        void Initialize(Engine::Window& window);

        void Update();

        void ProcessEvent(const InputEvent& evt);

        void SwapFrames();

        void OnOSKeyDown(KeyCode key);

        void OnOSKeyUp(KeyCode key);
        bool IsKeyHeld(KeyCode key);
        bool IsKeyJustPressed(KeyCode key);
        bool IsKeyJustReleased(KeyCode key);

        bool IsMouseButtonHeld(MouseButton button);
        bool IsMouseButtonJustPressed(MouseButton button);
        bool IsMouseButtonJustReleased(MouseButton button);
        glm::vec2 GetMouseDelta() const { return mouseDelta; }
        glm::vec2 GetMousePosition() const { return mousePosition; }

        void ProcessEvents(intptr_t ptr) {return backend->ProcessEvents(ptr);}
    };
}