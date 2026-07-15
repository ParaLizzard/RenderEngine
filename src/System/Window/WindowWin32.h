#pragma once
#include "Core/Exception.h"
#include "System/Window/WindowInterface.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>

namespace Engine {
    class WindowWin32 : public WindowInterface
    {
    public:
        class WindowException : public Engine::Exception
        {
        public:
            WindowException(int line, const char* file, HRESULT hr) noexcept;
            const char* what() const noexcept override;
            const char* getType() const noexcept override;
            static std::string translateErrorCode(HRESULT hr);
            HRESULT getErrorCode() const noexcept;
            std::string getErrorMessage() const noexcept;
        private:
            HRESULT hr;
        };

    private:
        class WindowClass
        {
        public:
            static const char* getName() noexcept;
            static HINSTANCE getInstance() noexcept;
        private:
            WindowClass() noexcept;
            ~WindowClass();
            WindowClass(const WindowClass&) = delete;
            WindowClass& operator=(const WindowClass&) = delete;
            static constexpr const char* windowClassName = "WindowClass";
            static WindowClass wndClass;
            HINSTANCE hInst;
        };

        class Timer {
        public:
            Timer() {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&start);
            }

            double getTime() const {
                LARGE_INTEGER current;
                QueryPerformanceCounter(&current);

                return static_cast<double>(current.QuadPart - start.QuadPart) / frequency.QuadPart;
            }

            void reset() {
                QueryPerformanceCounter(&start);
            }

        private:
            LARGE_INTEGER frequency;
            LARGE_INTEGER start;
        };

    public:
        WindowWin32(int width, int height, std::string title);
        ~WindowWin32() override;
        WindowWin32(const WindowWin32&) = delete;
        WindowWin32& operator=(const WindowWin32&) = delete;

        static std::unique_ptr<WindowInterface> createWindow(int width, int height, std::string title);

    private:
        void pollEvents() override;
        void setWindowTitle(std::string_view title) override;
        void setResizable(bool bResizable) override;
        void setWindowUserPointer(void *, void *pointer) override;
        double getTime() override
        {
            return timer.getTime();
        };
        void createWindowSurface(VkInstance instance, VkSurfaceKHR*surface) override;
        void *getWindowHandle() override
        {
            return hWnd;
        };
        VkExtent2D getExtent() override;
        bool shouldClose() override {return isClosing;};

        static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    private:
        int width, height;
        HWND hWnd;
        Timer timer{};

        bool isClosing = false;
    };

#define WND_EXCEPT(hr) WindowWin32::WindowException(__LINE__, __FILE__, hr)
#define WND_LAST_EXCEPT() WindowWin32::WindowException(__LINE__, __FILE__, GetLastError())
}
