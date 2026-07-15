#pragma once
#include <memory>
#include <string>
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

namespace Engine {
    class InputManager;

    class WindowInterface
    {
    public:
        virtual ~WindowInterface() = default;

        //virtual std::unique_ptr<WindowInterface> createWindow(int width, int height, std::string title) = 0;

        virtual void setWindowUserPointer(void*/*window handle*/, void* pointer) = 0;
        virtual void setWindowTitle(std::string_view title) = 0;

        virtual void pollEvents() = 0;

        virtual void setResizable(bool bResizable) = 0;
        virtual double getTime() = 0;
        virtual void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) = 0;
        virtual bool shouldClose() = 0;

        virtual void* getWindowHandle() = 0;
        virtual VkExtent2D getExtent() = 0;

        void setInputManager(InputManager * manager)
        {
            inputManager = manager;
        }

    protected:
        InputManager* inputManager = nullptr;
    };
}