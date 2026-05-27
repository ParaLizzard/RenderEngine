#pragma once
#include <cassert>
#include <memory>
#include "Core/Device.h"
#include "Core/Window.h"
#include "Swapchain.h"
#include <vulkan/vulkan.h>

namespace Engine
{
    struct FrameData
    {
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence fence;
    };

    class Renderer
    {
    public:
        static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
        Renderer(Window& window, Device& device);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        float getaspectRatio() const {return swapChain->extentAspectRatio();};
        bool isFrameInProgess() const {return isFrameStarted;}


        size_t getFrameIndex()
        {
            assert(isFrameStarted && "Cannot get frameindex when frame not in progress");
            return currentFrameIndex;
        };

        VkCommandBuffer beginFrame();
        void endFrame();
        VkCommandBuffer getCurrentCommandBuffer();
        SwapChain& getSwapChain();

        bool wasSwapChainRecreated() const { return swapChainRecreatedThisFrame; }
        std::unique_ptr<SwapChain> swapChain;
    private:
        std::vector<FrameData> frames;
        size_t currentFrameIndex = 0;
        uint32_t currentImageIndex = 0;
        bool isFrameStarted = false;
        bool swapChainRecreatedThisFrame = false;
        void createFrameData();
        void recreateSwapChain();

        Window& window;
        Device& device;


    };
}