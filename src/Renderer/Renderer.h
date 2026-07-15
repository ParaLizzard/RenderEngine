#pragma once
#include <cassert>
#include <memory>
#include <vulkan/vulkan.h>
#include "Vulkan/Device.h"
#include "System/Window/Window.h"
#include "Vulkan/Swapchain.h"

namespace Engine {
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
        Renderer(Window &window, Device &device);
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;

        float getAspectRatio() const
        {
            return swapChain->extentAspectRatio();
        };
        bool isFrameInProgess() const
        {
            return isFrameStarted;
        }


        size_t getFrameIndex()
        {
            assert(isFrameStarted && "Cannot get frameindex when frame not in progress");
            return currentFrameIndex;
        };

        VkCommandBuffer beginFrame();
        void endFrame();
        VkCommandBuffer getCurrentCommandBuffer();
        SwapChain &getSwapChain();
        uint32_t getCurrentImageIndex() const
        {
            return currentImageIndex;
        }
        int getFrameIndex() const
        {
            return currentFrameIndex;
        }

        bool wasSwapChainRecreated() const
        {
            return swapChainRecreatedThisFrame;
        }
        std::unique_ptr<SwapChain> swapChain;

    private:
        std::vector<FrameData> frames;
        size_t currentFrameIndex = 0;
        uint32_t currentImageIndex = 0;
        bool isFrameStarted = false;
        bool swapChainRecreatedThisFrame = false;
        void createFrameData();
        void recreateSwapChain();

        Window &window;
        Device &device;
    };
} // namespace Engine
