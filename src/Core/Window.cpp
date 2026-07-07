#include "Window.h"


namespace Engine {
    Window::Window(int width, int height, std::string title): width(width), height(height), title(title)
    {
        initWindow();
    }

    Window::~Window()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::initWindow()
    {
        if (!glfwInit()) {
            throw std::runtime_error("Window: glfwInit failed");
        }

        glfwSetErrorCallback(errorCallback);
        int vulkanSupported = glfwVulkanSupported();

        if (!vulkanSupported) {
            glfwTerminate();
            throw std::runtime_error("Window: glfw doesnt support vulkan API");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Window: Window creation failed");
        }

        glfwSetWindowUserPointer(window, this);
    }

    void Window::createWindowSurface(VkInstance Instance, VkSurfaceKHR *Surface)
    {
        if (glfwCreateWindowSurface(Instance, window, nullptr, Surface) != VK_SUCCESS) {
            throw std::runtime_error("Window: failed to create window surface");
        }
    }


    void Window::errorCallback(int error, const char *description)
    {
        fprintf(stderr, "Error: %s\n", description);
    }
} // namespace Engine
