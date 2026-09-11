#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "Vulkan/VulkanDevice.h"
#include "System/Window/IWindow.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/WindowEvents.h"

namespace Engine {
    class SwapChain
    {
    public:
        SwapChain(VulkanDevice &deviceRef, IWindow &window, std::shared_ptr<SwapChain> previous = nullptr);
        SwapChain(VulkanDevice &deviceRef, VkExtent2D windowExtent);
        SwapChain(VulkanDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous);
        ~SwapChain();

        SwapChain(const SwapChain &) = delete;
        SwapChain &operator=(const SwapChain &) = delete;

        VkImageView getImageView(int index)
        {
            return swapChainImageViews[index];
        }
        size_t imageCount()
        {
            return swapChainImages.size();
        }
        VkFormat getSwapChainImageFormat()
        {
            return swapChainImageFormat;
        }
        VkExtent2D getSwapChainExtent()
        {
            return swapChainExtent;
        }
        uint32_t width()
        {
            return swapChainExtent.width;
        }
        uint32_t height()
        {
            return swapChainExtent.height;
        }
        VkImage getImage(int index)
        {
            return swapChainImages[index];
        }
        VkImageView getDepthImageView()
        {
            return depthImageView;
        }
        VkImage getDepthImage()
        {
            return depthImage;
        }
        VkFormat getDepthFormat()
        {
            return VK_FORMAT_D32_SFLOAT;
        }


        float extentAspectRatio()
        {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }

        VkResult acquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t *imageIndex);
        VkResult presentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex);

        bool compareSwapFormats(const SwapChain &swapChain) const
        {
            return swapChain.swapChainImageFormat == swapChainImageFormat;
        }

        void onWindowResize(uint32_t width, uint32_t height)
        {
            windowExtent = { width, height };
        }

    private:
        void init();
        void createSwapChain();
        void createImageViews();
        void createDepthResources();

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

        VkImage depthImage = VK_NULL_HANDLE;
        VmaAllocation depthAllocation = VK_NULL_HANDLE;
        VkImageView depthImageView = VK_NULL_HANDLE;

        VulkanDevice &device;
        IWindow *window = nullptr;
        VkExtent2D windowExtent;

        VkSwapchainKHR swapChain;
        std::shared_ptr<SwapChain> oldSwapChain;
        ScopedSubscription resizeSubscription;
    };
} // namespace Engine
