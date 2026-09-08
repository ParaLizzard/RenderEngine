#pragma once
#include "System/Events/Event.h"
#include <vector>
#include <string>

namespace Engine {

    class WindowResizeEvent : public Event {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height) : width(width), height(height) {}
        ENGINE_NODISCARD uint32_t GetWidth() const noexcept { return width; }
        ENGINE_NODISCARD uint32_t GetHeight() const noexcept { return height; }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)

        ENGINE_NODISCARD std::string ToString() const override {
            return std::string(GetName()) + ": " + std::to_string(width) + "x" + std::to_string(height);
        }

    private:
        uint32_t width, height;
    };

    class WindowContentScaleEvent : public Event {
    public:
        WindowContentScaleEvent(float scaleX, float scaleY) : scaleX(scaleX), scaleY(scaleY) {}
        ENGINE_NODISCARD float GetScaleX() const noexcept { return scaleX; }
        ENGINE_NODISCARD float GetScaleY() const noexcept { return scaleY; }

        EVENT_CLASS_TYPE(WindowContentScale)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)

        ENGINE_NODISCARD std::string ToString() const override {
            return std::string(GetName()) + ": " + std::to_string(scaleX) + ", " + std::to_string(scaleY);
        }

    private:
        float scaleX, scaleY;
    };

    class WindowFocusEvent : public Event
    {
    public:
        explicit WindowFocusEvent(bool isFocused) : isFocused(isFocused) {}

        ENGINE_NODISCARD bool IsFocused() const noexcept { return isFocused; }

        EVENT_CLASS_TYPE(WindowFocused)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)
    private:
        bool isFocused;
    };

    class WindowIconifyEvent : public Event
    {
    public:
        explicit WindowIconifyEvent(bool isIconified) : isIconified(isIconified) {}

        ENGINE_NODISCARD bool IsIconified() const noexcept { return isIconified; }

        EVENT_CLASS_TYPE(WindowIconify)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)
    private:
        bool isIconified;
    };

    class WindowCloseEvent : public Event {
    public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)
    };

    class WindowDropFilesEvent : public Event {
    public:
        explicit WindowDropFilesEvent(std::vector<std::string> paths) : filePaths(std::move(paths)) {}
        ENGINE_NODISCARD const std::vector<std::string>& GetFiles() const noexcept { return filePaths; }

        EVENT_CLASS_TYPE(WindowDropFiles)
        EVENT_CLASS_CATEGORY(EventCategory::Window)

    private:
        std::vector<std::string> filePaths;
    };

} // namespace Engine