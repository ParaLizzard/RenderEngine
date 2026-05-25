//
// Created by Jan Varga on 25.05.2026.
//

#include "Window.h"



using namespace Engine;

void Window::initWindow()
{
    if (!glfwVulkanSupported())
    {
        throw std::runtime_error("Window: glfw doesnt support vulkan API");
    }

    if (!glfwInit)
    {
        throw std::runtime_error("Window: Window initialization failed");
    }
    glfwSetErrorCallback(errorCallback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, title.c_str(), nullptr, nullptr);
    if (!window)
    {
        throw std::runtime_error("Window: Window creation failed");
    }

    glfwSetWindowUserPointer(window, this);
}

void Window::createWindowSurface(VkInstance Instance, VkSurfaceKHR* Surface)
{
    if (glfwCreateWindowSurface(Instance, window, nullptr, Surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface");
    }
}


void Window::errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}