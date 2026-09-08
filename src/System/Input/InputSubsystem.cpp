#include "InputSubsystem.h"

#include <glm/common.hpp>
#include <glm/ext/quaternion_geometric.hpp>

#include "Core/Log.h"
#include "System/Events/EventDispatcher.h"

namespace Engine
{
    bool InputSubsystem::Initialize(SubsystemRegistry &registry)
    {
        auto& bus = EventDispatcher::Get();

        listenerTokens.push_back(bus.Subscribe<KeyPressedEvent>([this](KeyPressedEvent& e) { OnKeyPressed(e); }));
        listenerTokens.push_back(bus.Subscribe<KeyReleasedEvent>([this](KeyReleasedEvent& e) { OnKeyReleased(e); }));
        listenerTokens.push_back(bus.Subscribe<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e) { OnMouseButtonPressed(e); }));
        listenerTokens.push_back(bus.Subscribe<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& e) { OnMouseButtonReleased(e); }));
        listenerTokens.push_back(bus.Subscribe<MouseMovedEvent>([this](MouseMovedEvent& e) { OnMouseMoved(e); }));
        listenerTokens.push_back(bus.Subscribe<MouseScrolledEvent>([this](MouseScrolledEvent& e) { OnMouseScrolled(e); }));
        listenerTokens.push_back(bus.Subscribe<MouseRawDeltaEvent>([this](MouseRawDeltaEvent& e) { OnMouseRawDelta(e); }));


        PushContext("DefaultContext", false);

        currentKeys.reset();
        previousKeys.reset();
        currentMouseButtons.reset();
        previousMouseButtons.reset();
        mousePosition = { 0.0f, 0.0f };
        mouseDelta = { 0.0f, 0.0f };
        mouseScroll = 0.0f;

        return true;
    }

    void InputSubsystem::Update(float deltaTime)
    {
        ISubsystem::Update(deltaTime);

        previousKeys = currentKeys;
        previousMouseButtons = currentMouseButtons;
        mouseDelta = {0.0f, 0.0f};
        mouseScroll = 0.0f;
    }

    void InputSubsystem::Shutdown()
    {
        auto& bus = EventDispatcher::Get();
        for (auto token : listenerTokens) {
            bus.Unsubscribe(token);
        }
        listenerTokens.clear();
        contextStack.clear();
        globalActionBindings.clear();
        globalAxisBindings.clear();
    }

    void InputSubsystem::PushContext(std::string_view name, bool blockLower)
    {
        InputContext ctx;
        ctx.name = name;
        ctx.blockLowerContexts = blockLower;
        contextStack.push_back(ctx);
    }

    void InputSubsystem::PopContext()
    {
        if (contextStack.size() > 1) {
            contextStack.pop_back();
        }
    }

    std::string_view InputSubsystem::GetCurrentContextName() const noexcept
    {
        if (contextStack.empty()) { return {}; }
        return contextStack.back().name;
    }

    void InputSubsystem::BindAction(std::string_view actionName, const ActionBinding &binding)
    {
        if(!contextStack.empty()){
            contextStack.back().actionBindings[std::string(actionName)] = binding;
        }
        else{
            LOG_ERROR("Input", "Cannot bind action in empty context");
        }
    }

    void InputSubsystem::BindAxis(std::string_view axisName, const AxisBinding &binding)
    {
        if(!contextStack.empty()){
            contextStack.back().axisBindings[std::string(axisName)] = binding;
        }
        else{
            LOG_ERROR("Input", "Cannot bind axis in empty context");
        }
    }

    bool InputSubsystem::IsActionTriggered(std::string_view actionName) const
    {
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
            const auto &ctx = *it;

            auto itFound = ctx.actionBindings.find(actionName);
            if (itFound != ctx.actionBindings.end()) {
                const auto &binding = itFound->second;

                if (binding.modifierKey != KeyCode::Unknown && !IsKeyPressed(binding.modifierKey)) {
                    return false;
                }

                auto isKeyActive = [this, type = binding.triggerType](KeyCode key) -> bool {
                    switch (type) {
                    case TriggerType::Pressed:
                        return IsKeyPressed(key);
                    case TriggerType::JustPressed:
                        return IsKeyJustPressed(key);
                    case TriggerType::JustReleased:
                        return IsKeyJustReleased(key);
                    default:
                        return IsKeyPressed(key);
                    }
                };

                auto isMouseActive = [this, type = binding.triggerType](MouseButton btn) -> bool {
                    switch (type) {
                    case TriggerType::Pressed:
                        return IsMouseButtonPressed(btn);
                    case TriggerType::JustPressed:
                        return IsMouseButtonJustPressed(btn);
                    case TriggerType::JustReleased:
                        return IsMouseButtonJustReleased(btn);
                    default:
                        return IsMouseButtonPressed(btn);
                    }
                };

                if (binding.primaryKey != KeyCode::Unknown && isKeyActive(binding.primaryKey)) {
                    return true;
                }

                for (KeyCode key: binding.keys) {
                    if (isKeyActive(key)) {
                        return true;
                    }
                }

                for (MouseButton btn: binding.mouseButtons) {
                    if (isMouseActive(btn)) {
                        return true;
                    }
                }

                return false;
            }

            if (ctx.blockLowerContexts) {
                break;
            }
        }

        return false;
    }

    namespace {
        glm::vec2 ApplyScaledRadialDeadzone(glm::vec2 rawStick, float deadzoneMin = 0.15f, float deadzoneMax = 0.95f)
        {
            float magnitude = glm::length(rawStick);
            
            if (magnitude < 1e-5f || magnitude < deadzoneMin) {
                return glm::vec2(0.0f);
            }
            
            float scaledMagnitude = std::clamp((magnitude - deadzoneMin) / (deadzoneMax - deadzoneMin), 0.0f, 1.0f);
            return (rawStick / magnitude) * scaledMagnitude;
        }
    }

    float InputSubsystem::GetAxis(std::string_view axisName) const
    {
       
        for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
            const auto& ctx = *it;
            auto itFound = ctx.axisBindings.find(axisName);
            
            if (itFound != ctx.axisBindings.end()) {
                const auto& axis = itFound->second;
                float value = 0.0f;

                for (const auto& key : axis.keyScales) {
                    if (IsKeyPressed(key.key)) {
                        value += key.scale;
                    }
                }

                if (axis.positiveKey != KeyCode::Unknown && IsKeyPressed(axis.positiveKey)) {
                    value += axis.scale;
                }
                if (axis.negativeKey != KeyCode::Unknown && IsKeyPressed(axis.negativeKey)) {
                    value -= axis.scale;
                }
                
                float mouseFactor = axis.sensitivity * (axis.invert ? -1.0f : 1.0f);
                if (axis.useMouseDeltaX) {
                    value += mouseDelta.x * mouseFactor;
                }
                if (axis.useMouseDeltaY) {
                    value += mouseDelta.y * mouseFactor;
                }

                if (axis.gamepadAxis != GamepadAxis::Count) {
                    if (std::abs(value) < axis.deadzone) {
                        value = 0.0f;
                    } else {
                        float sign = (value > 0.0f) ? 1.0f : -1.0f;
                        value = sign * ((std::abs(value) - axis.deadzone) / (1.0f - axis.deadzone));
                    }
                }

                if (!axis.useMouseDeltaX && !axis.useMouseDeltaY) {
                    value = glm::clamp(value, -1.0f, 1.0f);
                }

                return value;
            }

            if (ctx.blockLowerContexts) {
                break;
            }
        }

        return 0.0f;
    }

    glm::vec2 InputSubsystem::GetVector2D(std::string_view axisX, std::string_view axisY) const
    {
        float x = GetAxis(axisX);
        float y = GetAxis(axisY);
        glm::vec2 v(x, y);

        v = ApplyScaledRadialDeadzone(v);

        float lengthSq = glm::dot(v, v);
        if (lengthSq > 1.0f) {
            v /= std::sqrt(lengthSq);
        }
        return v;
    }


    void InputSubsystem::OnKeyPressed(KeyPressedEvent &e)
    {
        auto key = static_cast<size_t>(e.GetKeyCode());
        if (key < currentKeys.size()) {
            currentKeys.set(key, true);
        }
    }

    void InputSubsystem::OnKeyReleased(KeyReleasedEvent &e)
    {
        auto key = static_cast<size_t>(e.GetKeyCode());
        if (key < currentKeys.size()) {
            currentKeys.set(key, false);
        }
    }

    void InputSubsystem::OnMouseButtonPressed(MouseButtonPressedEvent &e)
    {
        auto btn = static_cast<size_t>(e.GetMouseButton());
        if (btn < currentMouseButtons.size()) {
            currentMouseButtons.set(btn, true);
        }
    }

    void InputSubsystem::OnMouseButtonReleased(MouseButtonReleasedEvent &e)
    {
        auto btn = static_cast<size_t>(e.GetMouseButton());
        if (btn < currentMouseButtons.size()) {
            currentMouseButtons.set(btn, false);
        }
    }

    void InputSubsystem::OnMouseMoved(MouseMovedEvent &e)
    {
        mousePosition = {e.GetX(), e.GetY()};
    }

    void InputSubsystem::OnMouseScrolled(MouseScrolledEvent &e)
    {
        mouseScroll += e.GetOffsetY();
    }

    void InputSubsystem::OnMouseRawDelta(MouseRawDeltaEvent &e)
    {
        mouseDelta.x += e.GetDeltaX();
        mouseDelta.y += e.GetDeltaY();
    }
} // Engine