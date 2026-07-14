#include "InputManager.h"
#include "Window.h"

namespace Engine {

void InputManager::Initialize(Engine::Window& window) {
    backend->Initialize(window);
}

void InputManager::Update() {
    backend->PollGamepads();

    const auto& events = backend->GetEventQueue();

    std::queue<InputEvent> eventsCopy = events;
    while (!eventsCopy.empty()) {
        const auto& evt = eventsCopy.front();
        ProcessEvent(evt);
        eventsCopy.pop();
    }
    backend->ClearEventQueue();

    SwapFrames();
}

void InputManager::ProcessEvent(const InputEvent &evt)
{
    switch (evt.type) {
    case InputEventType::KeyDown:
        nextFrame[evt.data.keyboard.key].pressed = true;
        nextFrame[evt.data.keyboard.key].value = 1.0f;
        break;
    case InputEventType::KeyUp:
        nextFrame[evt.data.keyboard.key].released = true;
        nextFrame[evt.data.keyboard.key].value = 0.0f;
        break;
    case InputEventType::MouseButtonDown:
        nextFrame[evt.data.keyboard.key].pressed = true;
        nextFrame[evt.data.keyboard.key].value = 0.0f;
        break;
    case InputEventType::MouseButtonUp:
        nextFrame[evt.data.keyboard.key].released = true;
        nextFrame[evt.data.keyboard.key].value = 0.0f;
        break;

        // other
    }
}

void InputManager::SwapFrames() {
    currentFrame = nextFrame;
    for (auto& [key, source] : nextFrame) {
        source.pressed = false;
        source.released = false;
    }
}

void InputManager::OnOSKeyDown(KeyCode key) {
    auto& source = nextFrame[key];

    if (source.value < 0.5f) {
        source.pressed = true;
    }
    source.value = 1.0f;
}

void InputManager::OnOSKeyUp(KeyCode key) {
    auto& source = nextFrame[key];

    source.released = true;
    source.value = 0.0f;
}

bool InputManager::IsKeyHeld(KeyCode key) {
    return currentFrame[key].value > 0.5f;
}

bool InputManager::IsKeyJustPressed(KeyCode key) {
    return currentFrame[key].pressed;
}

bool InputManager::IsKeyJustReleased(KeyCode key) {
    return currentFrame[key].released;
}

} // namespace Engine