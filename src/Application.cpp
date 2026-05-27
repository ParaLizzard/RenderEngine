#include "Application.h"

#include <format>

namespace Engine
{
    Application::Application()
    {
        Texture2D testTexture;
        testTexture.createDefaultTexture(&device, 255, 0, 0, 255, resourceHeap);

        ResourceHeap::TextureHandle testHandle = resourceHeap.registerTexture(testTexture.descriptor);

        resourceHeap.flushPendingUpdates();

        std::cout << "Stage 2 Verification: Texture registered at index " << testHandle.index << std::endl;

        testTexture.destroy();
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        while (!window.shouldClose())
        {
            glfwPollEvents();

            VkCommandBuffer cb = device.beginSingleTimeCommands();

            // Clear the screen

            device.endSingleTimeCommands(cb);
        }
    }
}
