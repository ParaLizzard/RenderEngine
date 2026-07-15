#include "System/Window/Window.h"
#include "System/Window/WindowInterface.h"
#include "System/Window/WindowWin32.h"

namespace Engine {
    Window::Window(int width, int height, std::string title): width(width), height(height), title(title)
    {
        initWindow();
    }

    Window::~Window()
    {
    }

    void Window::initWindow()
    {
        window = WindowWin32::createWindow(width,height,title);
        window->setResizable(true);
    }

    void Window::createWindowSurface(VkInstance Instance, VkSurfaceKHR *Surface)
    {
        window->createWindowSurface(Instance,Surface);

    }

    void Window::setWindowTitle(std::string_view title)
    {
        window->setWindowTitle(title);
    }

    void Window::errorCallback(int error, const char *description)
    {
        fprintf(stderr, "Error: %s\n", description);
    }
} // namespace Engine
