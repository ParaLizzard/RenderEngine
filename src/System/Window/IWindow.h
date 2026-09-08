#pragma once
#include <string>
#include <string_view>
#include <vulkan/vulkan.h>

#include "Core/CoreDefines.h"

namespace Engine {
    enum class CursorMode {
        Normal,
        Hidden,
        Disabled
    };

    struct WindowProps {
        std::string title = "RenderEngine";
        uint32_t width = 3840;
        uint32_t height = 2120;
        bool vsync = false;
        bool fullscreen = false;
        bool resizable = true;
        CursorMode cursorMode = CursorMode::Normal;
    };

    class IWindow {
    public:
        virtual ~IWindow() = default;

        virtual void PollEvents() = 0;
        ENGINE_NODISCARD virtual bool ShouldClose() const = 0;
        virtual void RequestClose() = 0;

        ENGINE_NODISCARD virtual uint32_t GetWidth() const noexcept = 0;
        ENGINE_NODISCARD virtual uint32_t GetHeight() const noexcept = 0;
        ENGINE_NODISCARD virtual VkExtent2D GetExtent() const noexcept { return { GetWidth(), GetHeight() }; }
        ENGINE_NODISCARD virtual float GetAspectRatio() const noexcept { return static_cast<float>(GetWidth()) / static_cast<float>(GetHeight()); }

        virtual void SetTitle(std::string_view title) = 0;
        void setWindowTitle(std::string_view title) { SetTitle(title); }
        virtual void SetVSync(bool enabled) = 0;
        ENGINE_NODISCARD virtual bool IsVSync() const noexcept = 0;
        virtual void SetCursorMode(CursorMode mode) = 0;

        virtual bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* outSurface) = 0;
        ENGINE_NODISCARD virtual void* GetNativeHandle() const noexcept = 0;
    };
}
