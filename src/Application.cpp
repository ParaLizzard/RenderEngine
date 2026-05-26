#include "Application.h"

namespace Engine
{
    Application::Application()
    {
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        while(!window.shouldClose())
        {
            glfwPollEvents();

            VkCommandBuffer cb = device.beginSingleTimeCommands();

            // Clear the screen

            device.endSingleTimeCommands(cb);
        }
    }
}