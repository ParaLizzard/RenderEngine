#pragma once
#include "System/Events/Event.h"
#include <vector>
#include <string>

namespace Engine {

    class WindowResizeEvent : public Event {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height) : width(width), height(height) {}
        uint32_t GetWidth() const noexcept { return width; }
        uint32_t GetHeight() const noexcept { return height; }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategory::Window | EventCategory::Application)

        std::string ToString() const override {
            return std::string(GetName()) + ": " + std::to_string(width) + "x" + std::to_string(height);
        }

    private:
        uint32_t width, height;
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
        const std::vector<std::string>& GetFiles() const noexcept { return filePaths; }

        EVENT_CLASS_TYPE(WindowDropFiles)
        EVENT_CLASS_CATEGORY(EventCategory::Window)

    private:
        std::vector<std::string> filePaths;
    };

} // namespace Engine