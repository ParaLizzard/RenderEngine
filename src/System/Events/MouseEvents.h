#pragma once
#include "System/Events/Event.h"
#include "System/Input/InputCodes.h"

namespace Engine {

    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(float x, float y) : mouseX(x), mouseY(y) {}
        ENGINE_NODISCARD float GetX() const noexcept { return mouseX; }
        ENGINE_NODISCARD float GetY() const noexcept { return mouseY; }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

    private:
        float mouseX, mouseY;
    };

    class MouseRawDeltaEvent : public Event {
    public:
        MouseRawDeltaEvent(float dx, float dy) : deltaX(dx), deltaY(dy) {}
        ENGINE_NODISCARD float GetDeltaX() const noexcept { return deltaX; }
        ENGINE_NODISCARD float GetDeltaY() const noexcept { return deltaY; }

        EVENT_CLASS_TYPE(MouseRawDelta)
        EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

    private:
        float deltaX, deltaY;
    };

    class MouseScrolledEvent : public Event {
    public:
        MouseScrolledEvent(float xOffset, float yOffset) : xOffset(xOffset), yOffset(yOffset) {}
        ENGINE_NODISCARD float GetOffsetX() const noexcept { return xOffset; }
        ENGINE_NODISCARD float GetOffsetY() const noexcept { return yOffset; }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Input)

    private:
        float xOffset, yOffset;
    };

    class MouseEnterEvent : public Event
    {
    public:
        explicit MouseEnterEvent(bool entered) : entered(entered) {}

        ENGINE_NODISCARD bool IsIconified() const noexcept { return entered; }

        EVENT_CLASS_TYPE(MouseEnter)
        EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::Application)
    private:
        bool entered;
    };

    class MouseButtonEvent : public Event {
    public:
        ENGINE_NODISCARD MouseButton GetMouseButton() const noexcept { return button; }
        EVENT_CLASS_CATEGORY(EventCategory::Mouse | EventCategory::MouseButton | EventCategory::Input)

    protected:
        explicit MouseButtonEvent(MouseButton btn) : button(btn) {}
        MouseButton button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent {
    public:
        explicit MouseButtonPressedEvent(MouseButton btn) : MouseButtonEvent(btn) {}

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent {
    public:
        explicit MouseButtonReleasedEvent(MouseButton btn) : MouseButtonEvent(btn) {}

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

} // namespace Engine