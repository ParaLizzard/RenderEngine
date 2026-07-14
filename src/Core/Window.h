#pragma once
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan.h>

#include "WindowInterface.h"

namespace Engine {
    class InputManager;

    class Window
    {
    public:
        Window(int width, int height, std::string title);
        ~Window();

        Window(Window const &) = delete;
        Window &operator=(Window const &) = delete;

        void initWindow();
        void createWindowSurface(VkInstance Instance, VkSurfaceKHR *Surface);
        bool shouldClose()
        {
            return window->shouldClose();
        }

        VkExtent2D getExtent()
        {
            return VkExtent2D(width, height);
        };

        std::unique_ptr<WindowInterface>& getWindow()
        {
            return window;
        };

        void setWindowTitle(std::string_view title);
        void pollEvents(){window->pollEvents();}
        double getTime(){return window->getTime();}
        void* getWindowHandle() {return window->getWindowHandle();}
        void setInputManager(InputManager* manager) {window->setInputManager(manager);}

    private:
        static void errorCallback(int error, const char *description);

        std::string title;
        int width, height;

        std::unique_ptr<WindowInterface> window;
    };
} // namespace Engine