#pragma once
#include "System/Input/InputCodes.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "Core/Hash.h"

namespace Engine {

    enum class TriggerType : uint8_t {
        Pressed,
        JustPressed,
        JustReleased,
        Hold,
        DoubleTap
    };

    struct ActionBinding {
        std::vector<KeyCode> keys;
        std::vector<MouseButton> mouseButtons;
        std::vector<GamepadButton> gamepadButtons;
        KeyCode primaryKey = KeyCode::Unknown;
        KeyCode modifierKey = KeyCode::Unknown;

        TriggerType triggerType = TriggerType::Pressed;
        float holdDuration = 0.5f;
        float doubleTapThreshold = 0.25f;
    };

    struct AxisKeyScale {
        KeyCode key = KeyCode::Unknown;
        float scale = 1.0f;
    };

    struct AxisBinding {
        std::vector<AxisKeyScale> keyScales;
        KeyCode positiveKey = KeyCode::Unknown;
        KeyCode negativeKey = KeyCode::Unknown;
        float scale = 1.0f;
        bool useMouseDeltaX = false;
        bool useMouseDeltaY = false;
        GamepadAxis gamepadAxis = GamepadAxis::Count;
        float deadzone = 0.15f;
        float sensitivity = 1.0f;
        bool invert = false;
    };

    struct InputContext {
        std::string name;
        int32_t priority = 0;
        bool blockLowerContexts = false;
        TransparentStringMap<ActionBinding> actionBindings;
        TransparentStringMap<AxisBinding> axisBindings;
    };



} // namespace Engine