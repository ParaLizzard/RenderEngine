#include "Renderer.h"

namespace Engine
{
    Renderer::Renderer(Window& window, Device& device): window(window), device(device)
    {
        swapChain = std::make_unique(device, VkExtent2D(600,800));

        createFrameData();
    }

    Renderer::~Renderer()
    {
    }

    VkCommandBuffer Renderer::beginFrame()
    {
    }

    void Renderer::endFrame()
    {
    }

    VkCommandBuffer Renderer::getCurrentCommandBuffer()
    {
    }

    SwapChain& Renderer::getSwapChain()
    {
    }

    void Renderer::createFrameData()
    {
    }

    void Renderer::recreateSwapChain()
    {
    }
}
