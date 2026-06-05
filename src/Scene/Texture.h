#pragma once

#include "Core/Device.h"
#include <ktxvulkan.h>
#include <filesystem>
#include "Core/Buffer.h"
#include <cassert>
#include <iostream>
#include <cmath>
#include <array>
#include "Renderer/ResourceHeap.h"

namespace Engine
{
    class Texture
    {
    public:
        Device* device = nullptr;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        uint32_t width = 0, height = 0;
        uint32_t mipLevels = 0;
        uint32_t layerCount = 0;
        VkDescriptorImageInfo descriptor{};
        VkSampler sampler = VK_NULL_HANDLE;
        ResourceHeap::TextureHandle heapHandle{};

        Texture() = default;

        virtual ~Texture() { destroy(); }

        Texture(Texture&& other) noexcept
        {
            *this = std::move(other);
        }

        Texture& operator=(Texture&& other) noexcept
        {
            if (this != &other)
            {
                destroy();

                device = other.device;
                image = other.image;
                imageLayout = other.imageLayout;
                allocation = other.allocation;
                view = other.view;
                width = other.width;
                height = other.height;
                mipLevels = other.mipLevels;
                layerCount = other.layerCount;
                descriptor = other.descriptor;
                sampler = other.sampler;
                heapHandle = other.heapHandle;

                other.image = VK_NULL_HANDLE;
                other.view = VK_NULL_HANDLE;
                other.sampler = VK_NULL_HANDLE;
                other.allocation = VK_NULL_HANDLE;
                other.device = nullptr;
            }
            return *this;
        }

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

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

        void fromKTXPtr(
            void* ktxTexPtr,
            Device* device,
            ResourceHeap& resourceHeap,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        void createDefaultTexture(Device* device, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                  ResourceHeap& resourceHeap);
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

        void loadFromFileSTB(
            std::array<std::string, 6> filenames,
            VkFormat format,
            Device* device,
            ResourceHeap& resourceHeap,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };
}
