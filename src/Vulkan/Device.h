#pragma once
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"

namespace Engine {
    class Window;

    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool isComplete()
        {
            return graphicsFamilyHasValue && presentFamilyHasValue;
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class Device
    {
    public:
        Device(Window &window);
        ~Device();

        Device(Device const &) = delete;
        Device &operator=(Device const &) = delete;

        VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,
                                     VkImageTiling tiling,
                                     VkFormatFeatureFlags features);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void createImageWithInfo(const VkImageCreateInfo &imageInfo, VkImage &image, VmaAllocation &allocation);
        void checkGlfwRequiredExtensions();
        VkPhysicalDeviceProperties getDeviceProperties();
        VkFormatProperties getFormatProperties(VkFormat format);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);

        VkDevice getDevice()
        {
            return device;
        }
        VkInstance getInstance()
        {
            return instance;
        }
        VkPhysicalDevice getPhysicalDevice()
        {
            return physicalDevice;
        }
        VkCommandPool getCommandPool()
        {
            return commandPool;
        }
        VmaAllocator getAllocator()
        {
            return allocator;
        }
        VkPipelineCache getPipelineCache()
        {
            return pipelineCache;
        }
        float getMaxAnisotropy();
        uint32_t getGraphicsFamilyIndex()
        {
            return indices.graphicsFamily;
        }
        uint32_t getPresentFamilyIndex()
        {
            return indices.presentFamily;
        }
        VkQueue getGraphicsQueue()
        {
            return graphicsQueue;
        }
        VkQueue getPresentQueue()
        {
            return presentQueue;
        }
        VkSurfaceKHR getSurface()
        {
            return surface;
        }
        void transitionImageLayout(VkCommandBuffer commandBuffer,
                                   VkImage image,
                                   VkImageLayout oldLayout,
                                   VkImageLayout newLayout,
                                   VkImageSubresourceRange subresourceRange);

    private:
        bool enableValidationLayers = false;

        VkDevice device = VK_NULL_HANDLE;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
        Window &window;
        VkDebugUtilsMessengerEXT debugMessenger;

        QueueFamilyIndices indices;
        VkSurfaceKHR surface;
        VkQueue graphicsQueue;
        VkQueue presentQueue;

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        void createInstance();
        std::vector<const char *> getRequiredExtensions();
        void setupDebugMessenger();
        void populateDebugMessageInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void createSurface();
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);

        void createLogicalDevice();
        void createCommandPool();
        void createAllocator();
        void createPipelineCache();
        void savePipelineCache();

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
    };
} // namespace Engine