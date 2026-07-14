#pragma once
#include <glm/vec2.hpp>

enum class InputEventType {
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseDelta,
    MouseScroll,
    GamepadButtonDown,
    GamepadButtonUp,
    GamepadAxisMotion,
    GamepadTriggerMotion,
};

enum class KeyCode : uint32_t {
    // Printable keys
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Special keys
    Space,
    Tab,
    Enter,
    Escape,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,

    // Arrow keys
    Up,
    Down,
    Left,
    Right,

    // Modifiers
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Numpad
    NumpadAdd,
    NumpadSubtract,
    NumpadMultiply,
    NumpadDivide,
    NumpadDecimal,
    NumpadEnter,

    // Caps lock, Scroll lock, etc.
    CapsLock,
    ScrollLock,
    NumLock,

    // Invalid/unknown
    Unknown = 0xFFFFFFFF,
};

enum class MouseButton : uint32_t {
    Left,
    Right,
    Middle,
    X1,
    X2,
    Unknown = 0xFFFFFFFF,
};

enum class GamepadButton : uint32_t {
    A,
    B,
    X,
    Y,
    LB,
    RB,
    Back,
    Start,
    LeftThumb,
    RightThumb,
    Guide,  // Xbox button, PS button, etc.
    Unknown = 0xFFFFFFFF,
};

enum class GamepadAxis : uint32_t {
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    Unknown = 0xFFFFFFFF,
};

enum class GamepadTrigger : uint32_t {
    Left,
    Right,
    Unknown = 0xFFFFFFFF,
};

struct InputBinding {
    InputEventType type;
    uint32_t code;           // Key code, button ID, etc.
    float deadzone = 0.0f;   // For analog: min threshold to register
    float sensitivity = 1.0f;
    bool inverted = false;
};

struct InputSource {
    InputBinding binding;
    float value = 0.0f;      // 0.0 = not pressed, 1.0 = pressed/max
    float rawValue = 0.0f;   // Before deadzone/sensitivity
    bool pressed = false;    // Transitioned from 0→1 this frame
    bool released = false;   // Transitioned from 1→0 this frame
    uint64_t pressTime = 0;  // Frame number input was pressed
};

struct InputEvent {
    InputEventType type;
    uint64_t timestamp;  // Frame number or high-res clock

    // Discriminated union for event data
    union {
        struct {
            KeyCode key;
            std::int32_t scancode;
            uint32_t mods;  // Shift, Ctrl, Alt state
        } keyboard;

        struct {
            MouseButton button;
            int32_t x, y;  // Absolute position at time of click
        } mouseButton;

        struct {
            int32_t x, y;          // Absolute position
            int32_t deltaX, deltaY;  // Relative movement (if backend supports raw input)
        } mouseMotion;

        struct {
            float delta;  // Scroll amount (positive = up, negative = down)
        } mouseScroll;

        struct {
            uint32_t gamepadId;
            GamepadButton button;
        } gamepadButton;

        struct {
            uint32_t gamepadId;
            GamepadAxis axis;
            glm::vec2 value;  // -1.0 to 1.0
        } gamepadAxis;

        struct {
            uint32_t gamepadId;
            GamepadTrigger trigger;
            float value;  // 0.0 to 1.0
        } gamepadTrigger;
    } data;
};