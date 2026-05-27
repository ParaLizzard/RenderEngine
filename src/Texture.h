#pragma once

#include "Device.h"
#include "ktx.h"
#include "filesystem"

namespace Engine
{
    class Texture
    {
    public:
        Device* device;
        VkImage image;
        VkImageLayout imageLayout;
        VmaAllocation allocation;
        VkImageView view;
        uint32_t width, height;
        uint32_t mipLevels;
        uint32_t layerCount;
        VkDescriptorImageInfo descriptor;
        VkSampler sampler;

        void updateDescriptor();
        void destroy();

        ktxResult loadKTXFile(std::string filename, ktxTexture** target);

        void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
        void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

        void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkImageSubresourceRange subresourceRange);

        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin);
    };

    class Texture2D : public Texture
    {
    public:
    void loadFromFile(
            std::string filename,
            VkFormat format,
            Device* device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void fromBuffer(
        void* buffer,
        VkDeviceSize bufferSize,
        VkFormat format,
        uint32_t texWidth,
        uint32_t texHeight,
        Device& device,
        VkQueue copyQueue,
        VkFilter filter = VK_FILTER_LINEAR,
        VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void createDefaultTexture(Device& device, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    };

    class Texture2DArray : public Texture
    {
    public:
        void loadFromFile(
            std::string filename,
            VkFormat format,
            Device& device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };

    class TextureCubeMap : public Texture
    {
    public:
        void loadFromFile(
            std::string filename,
            VkFormat format,
            Device& device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };
}

