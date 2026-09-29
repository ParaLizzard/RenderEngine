#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/VulkanDevice.h"
#include "System/Window/IWindow.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include <algorithm>
#include <limits>

namespace Engine {
    VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, IWindow& window, VkSurfaceKHR surface, bool vsync)
        : device(device), window(window), surface(surface)
    {
        Create(window.GetWidth(), window.GetHeight(), vsync);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        Destroy();
    }

    void VulkanSwapchain::Recreate(uint32_t newWidth, uint32_t newHeight, bool vsync)
    {
        device.WaitIdle();
        Destroy();
        Create(newWidth, newHeight, vsync);
    }

    void VulkanSwapchain::Destroy()
    {
        images.clear();
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device.GetHandle(), swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanSwapchain::Create(uint32_t width, uint32_t height, bool vsync)
    {
        if (surface == VK_NULL_HANDLE) {
            return;
        }

        SwapChainSupportDetails details = device.QuerySwapChainSupport(surface);
        ENGINE_ASSERT(details.IsAdequate(), "Swap chain support is not adequate");

        surfaceFormat = details.formats[0];
        for (const auto& format : details.formats) {
            if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = format;
                break;
            }
        }

        if (vsync) {
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
        } else {
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
            for (const auto& mode : details.presentModes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    presentMode = mode;
                    break;
                }
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    presentMode = mode;
                }
            }
        }

        if (details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent = details.capabilities.currentExtent;
        } else {
            extent.width = std::clamp(width, details.capabilities.minImageExtent.width, details.capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, details.capabilities.minImageExtent.height, details.capabilities.maxImageExtent.height);
        }

        if (extent.width == 0 || extent.height == 0) {
            return;
        }

        uint32_t imageCount = details.capabilities.minImageCount + 1;
        if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
            imageCount = details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const auto& indices = device.GetQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = details.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(device.GetHandle(), &createInfo, nullptr, &swapchain);
        ENGINE_ASSERT(result == VK_SUCCESS, "Failed to create swap chain");

        uint32_t actualImageCount = 0;
        vkGetSwapchainImagesKHR(device.GetHandle(), swapchain, &actualImageCount, nullptr);
        std::vector<VkImage> vkImages(actualImageCount);
        vkGetSwapchainImagesKHR(device.GetHandle(), swapchain, &actualImageCount, vkImages.data());

        images.clear();
        images.reserve(actualImageCount);
        for (uint32_t i = 0; i < actualImageCount; ++i) {
            TextureDesc texDesc{};
            texDesc.debugName = "SwapchainImage";
            texDesc.extent = { extent.width, extent.height, 1 };
            texDesc.format = surfaceFormat.format;
            texDesc.usage = TextureUsage::ColorAttachment;
            texDesc.mipLevels = 1;
            texDesc.arrayLayers = 1;
            images.push_back(std::make_unique<VulkanTexture>(device, vkImages[i], texDesc));
        }
    }

    VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* outImageIndex)
    {
        if (swapchain == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        return vkAcquireNextImageKHR(device.GetHandle(), swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, outImageIndex);
    }

    VkResult VulkanSwapchain::Present(VkQueue presentQueue, uint32_t imageIndex, VkSemaphore renderFinishedSemaphore)
    {
        if (swapchain == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        }
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(presentQueue, &presentInfo);
    }
}
