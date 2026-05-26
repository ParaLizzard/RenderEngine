#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"

#include "Window.h"

namespace Engine
{
    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool isComplete() {return graphicsFamilyHasValue && presentFamilyHasValue;}
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
        Device(Window& window);
        ~Device();

        Device(Device const&) = delete;
        Device& operator=(Device const&) = delete;

        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                     VkFormatFeatureFlags features);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VmaMemoryUsage memUsage, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* pResultInfo);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);
        void createImageWithInfo(const VkImageCreateInfo& imageInfo, VkImage& image, VmaAllocation& allocation);
        void hasGflwRequiredInstanceExtensions();
        VkPhysicalDeviceProperties getDeviceProperties();
        VkFormatProperties getFormatProperties(VkFormat format);

        VkDevice getDevice(){return device;}
        VkInstance getInstance(){return instance;}
        VkPhysicalDevice getPhysicalDevice(){return physicalDevice;}
        VkCommandPool getCommandPool(){return commandPool;}
        VmaAllocator getAllocator(){return allocator;}

    private:
        bool enableValidationLayers = true;

        VkDevice device = VK_NULL_HANDLE;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
        Window &window;
        VkDebugUtilsMessengerEXT debugMessenger;

        VkSurfaceKHR surface_;
        VkQueue graphicsQueue_;
        VkQueue presentQueue_;

        void createInstance();
        std::vector<const char*> getRequiredExtensions();
        void setupDebugMessenger();
        void populateDebugMessageInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        void createSurface();
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
        void createLogicalDevice();
        void createCommandPool();
        void createAllocator();


        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    };
}

