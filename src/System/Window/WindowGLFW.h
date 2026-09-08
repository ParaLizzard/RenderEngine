#pragma once
#define GLFW_INCLUDE_VULKAN
#include "Core/Types.h"
#include "System/Window/IWindow.h"
#include "GLFW/glfw3.h"

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <dwmapi.h>
#endif

namespace Engine {
    class WindowGLFW : public IWindow {
    public:
        WindowGLFW(const WindowProps& props);
        ~WindowGLFW() override;


        void PollEvents() override;
        ENGINE_NODISCARD bool ShouldClose() const override { return shouldClose || (hwnd && glfwWindowShouldClose(hwnd)); }
        void RequestClose() override { shouldClose = true; }

        ENGINE_NODISCARD uint32_t GetWidth() const noexcept override { return properties.width; }
        ENGINE_NODISCARD uint32_t GetHeight() const noexcept override { return properties.height; }

        void SetTitle(std::string_view title) override;
        void SetVSync(bool enabled) override {
            if (properties.vsync != enabled) {
                properties.vsync = enabled;
            }
        }
        ENGINE_NODISCARD bool IsVSync() const noexcept override { return properties.vsync; }
        void SetCursorMode(CursorMode mode) override;

        bool CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* outSurface) override;
        ENGINE_NODISCARD void* GetNativeHandle() const noexcept override { return static_cast<void*>(hwnd); }

    private:
        void ApplyDpiScaling();
        void EnableDarkMode();

        void SetCallbacks();

        GLFWwindow* hwnd;
        WindowProps properties;
        bool shouldClose = false;
        CursorMode cursorMode = CursorMode::Normal;
        Vec2 dpiScale = {1.0f, 1.0f};
    };
}