#pragma once

#include "Core/Device.h"
#include <ktxvulkan.h>
#include <filesystem>
#include "Core/Buffer.h"
#include <cassert>
#include <iostream>
#include <cmath>
#include "Renderer/ResourceHeap.h"

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
        ResourceHeap::TextureHandle heapHandle;

        void updateDescriptor();
        void destroy();

        ktxResult loadKTXFile(std::string filename, ktxTexture** target);

        void transitionImageLayout(
            VkCommandBuffer commandBuffer,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkImageSubresourceRange subresourceRange);
    };

    class Texture2D : public Texture
    {
    public:
        void loadFromFile(
            std::string filename,
            VkFormat format,
            Device* device,
            ResourceHeap& resourceHeap,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
        void fromBuffer(
            void* buffer,
            VkDeviceSize bufferSize,
            VkFormat format,
            uint32_t texWidth,
            uint32_t texHeight,
            Device* device,
            ResourceHeap& resourceHeap,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        void createDefaultTexture(Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a,ResourceHeap& resourceHeap);
    };

    class Texture2DArray : public Texture
    {
    public:
        void loadFromFile(
            std::string filename,
            VkFormat format,
            Device* device,
            ResourceHeap& resourceHeap,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };

    class TextureCubeMap : public Texture
    {
    public:
        void loadFromFile(
            std::string filename,
            VkFormat format,
            Device* device,
            ResourceHeap& resourceHeap,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };
}
