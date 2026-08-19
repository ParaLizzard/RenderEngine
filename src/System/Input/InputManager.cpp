#include "System/Input/InputManager.h"
#include "System/Window/Window.h"

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
        mouseNextFrame[evt.data.mouseButton.button].pressed = true;
        mouseNextFrame[evt.data.mouseButton.button].value = 1.0f;
        break;
    case InputEventType::MouseButtonUp:
        mouseNextFrame[evt.data.mouseButton.button].released = true;
        mouseNextFrame[evt.data.mouseButton.button].value = 0.0f;
        break;
    case InputEventType::MouseMove:
        accumulatedMouseDelta.x += static_cast<float>(evt.data.mouseMotion.deltaX);
        accumulatedMouseDelta.y += static_cast<float>(evt.data.mouseMotion.deltaY);
        mousePosition.x = static_cast<float>(evt.data.mouseMotion.x);
        mousePosition.y = static_cast<float>(evt.data.mouseMotion.y);
        break;
    default:
        break;
    }
}

void InputManager::SwapFrames() {
    currentFrame = nextFrame;
    for (auto& [key, source] : nextFrame) {
        source.pressed = false;
        source.released = false;
    }

    mouseCurrentFrame = mouseNextFrame;
    for (auto& [button, source] : mouseNextFrame) {
        source.pressed = false;
        source.released = false;
    }

    mouseDelta = accumulatedMouseDelta;
    accumulatedMouseDelta = glm::vec2(0.0f);
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

bool InputManager::IsMouseButtonHeld(MouseButton button) {
    auto it = mouseCurrentFrame.find(button);
    return it != mouseCurrentFrame.end() && it->second.value > 0.5f;
}

bool InputManager::IsMouseButtonJustPressed(MouseButton button) {
    auto it = mouseCurrentFrame.find(button);
    return it != mouseCurrentFrame.end() && it->second.pressed;
}

bool InputManager::IsMouseButtonJustReleased(MouseButton button) {
    auto it = mouseCurrentFrame.find(button);
    return it != mouseCurrentFrame.end() && it->second.released;
}

} // namespace Engine