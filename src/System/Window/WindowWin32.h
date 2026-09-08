#pragma once
#include "System/Window/IWindow.h"
#include <windows.h>

#include "Core/Types.h"

namespace Engine {
    class WindowWin32 : public IWindow {
    public:
        WindowWin32(const WindowProps& props);
        ~WindowWin32() override;

        void PollEvents() override;
        ENGINE_NODISCARD bool ShouldClose() const override { return shouldClose; }
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
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

        void ApplyDpiScaling();
        void EnableDarkMode();

        HWND hwnd = nullptr;
        HINSTANCE hInstance = nullptr;
        WindowProps properties;
        bool shouldClose = false;
        bool mouseHovering = false;
        CursorMode cursorMode = CursorMode::Normal;
        Vec2 dpiScale = {1.0f, 1.0f};
    };
}
