#pragma once
#include "System/Events/Event.h"
#include "System/Input/InputCodes.h"

namespace Engine {

    class KeyEvent : public Event {
    public:
        ENGINE_NODISCARD KeyCode GetKeyCode() const noexcept { return keyCode; }
        EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

    protected:
        explicit KeyEvent(KeyCode code) : keyCode(code) {}
        KeyCode keyCode;
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        explicit KeyPressedEvent(KeyCode code, bool isRepeat = false) : KeyEvent(code), repeat(isRepeat) {}
        ENGINE_NODISCARD bool IsRepeat() const noexcept { return repeat; }

        EVENT_CLASS_TYPE(KeyPressed)

    private:
        bool repeat = false;
    };

    class KeyReleasedEvent : public KeyEvent {
    public:
        explicit KeyReleasedEvent(KeyCode code) : KeyEvent(code) {}

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class KeyTypedEvent : public Event {
    public:
        explicit KeyTypedEvent(uint32_t charCode) : character(charCode) {}
        ENGINE_NODISCARD uint32_t GetCharacter() const noexcept { return character; }

        EVENT_CLASS_TYPE(KeyTyped)
        EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)

    private:
        uint32_t character;
    };

} // namespace Engine