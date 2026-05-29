#include "Swapchain.h"


namespace Engine
{
    SwapChain::SwapChain(Device& device, VkExtent2D windowExtent):device(device), windowExtent(windowExtent)
    {
        init();
    }

    SwapChain::SwapChain(Device& device, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous):device(device), windowExtent(windowExtent)
    {
        oldSwapChain = std::move(previous);
        init();

    }

    SwapChain::~SwapChain()
    {
        if (depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device.getDevice(), depthImageView, nullptr);
        }
        if (depthImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(device.getAllocator(), depthImage, depthAllocation);
        }

        for (auto view : swapChainImageViews)
        {
            vkDestroyImageView(device.getDevice(), view, nullptr);
        }

        if (swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device.getDevice(), swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
        }
    }

    VkResult SwapChain::acquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex)
    {
        return vkAcquireNextImageKHR(device.getDevice(), swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, imageIndex);
    }

    VkResult SwapChain::presentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex)
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        return vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);
    }

    void SwapChain::init()
    {
        createSwapChain();
        createImageViews();
        createDepthResources();
    }

    void SwapChain::createSwapChain()
    {
        SwapChainSupportDetails details = device.querySwapChainSupport(device.getPhysicalDevice());

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
        VkExtent2D extent = chooseSwapExtent(details.capabilities);

        uint32_t imageCount = details.capabilities.minImageCount + 1;
        if (imageCount > details.capabilities.maxImageCount)
        {
            imageCount = details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapChainInfo{};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.surface = device.getSurface();
        swapChainInfo.minImageCount = imageCount;
        swapChainInfo.imageFormat = surfaceFormat.format;
        swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapChainInfo.imageExtent = extent;
        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t graphicsFamilyIndex = device.getGraphicsFamilyIndex();
        uint32_t presentFamilyIndex = device.getPresentFamilyIndex();

        if (graphicsFamilyIndex == presentFamilyIndex)
        {
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        } else
        {
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapChainInfo.queueFamilyIndexCount = 2;

            uint32_t queueFamilyIndices[2] = {graphicsFamilyIndex, presentFamilyIndex};
            swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        swapChainInfo.preTransform = details.capabilities.currentTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.presentMode = presentMode;
        swapChainInfo.clipped = VK_TRUE;

        swapChainInfo.oldSwapchain = oldSwapChain ? oldSwapChain->swapChain : VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device.getDevice(), &swapChainInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("SwapChain: failed to create family swap chain");
        }

        vkGetSwapchainImagesKHR(device.getDevice(), swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device.getDevice(), swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void SwapChain::createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());
        for (uint32_t i = 0; i < swapChainImages.size(); ++i)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainImageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;

            if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &swapChainImageViews[i])!= VK_SUCCESS)
            {
                throw std::runtime_error("SwapChain: failed to create image views");
            }
        }
    }

    VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (auto& presentMode : availablePresentModes)
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return presentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        VkExtent2D extent = windowExtent;

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }

    void SwapChain::createDepthResources()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_D32_SFLOAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        device.createImageWithInfo(imageInfo, depthImage, depthAllocation);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("SwapChain: Failed to create depth image view");
        }
    }
} // Engine