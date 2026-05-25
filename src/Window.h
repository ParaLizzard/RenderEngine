#pragma once
#define GLFW_INCLUDE_VULKAN
#include <string>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <stdexcept>



namespace Engine
{
    class Window
    {
    public:

        Window(int width, int height, std::string title);
        ~Window();

        Window(Window const&) = delete;
        Window& operator=(Window const&) = delete;

        void initWindow();
        void createWindowSurface(VkInstance Instance, VkSurfaceKHR* Surface);
        bool shouldClose() {return glfwWindowShouldClose(window);}

    private:
        static void errorCallback(int error, const char* description);

        std::string title;
        int width, height;

        GLFWwindow* window;
    };
}
