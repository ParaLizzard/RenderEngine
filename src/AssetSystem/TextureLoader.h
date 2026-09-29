#pragma once
#include <string_view>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include "Vulkan/VulkanTexture.h"
#include "Vulkan/VulkanBindlessHeap.h"

namespace Engine {
    class VulkanDevice;
    class VulkanMemory;

    struct DecodedImageData {
        std::vector<uint8_t> pixelData;
        VkExtent3D extent{ 0, 0, 1 };
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        bool isCompressed = false;
        std::vector<VkBufferImageCopy> copyRegions;
    };

    struct LoadedTexture {
        std::unique_ptr<VulkanTexture> texture;
        BindlessTextureHandle handle;
    };

    class TextureLoader {
    public:
        static DecodedImageData LoadFromFile(const std::filesystem::path& path, bool srgb = true);

        static DecodedImageData LoadFromMemory(std::span<const uint8_t> data, std::string_view extension, bool srgb = true);

        static std::unique_ptr<VulkanTexture> UploadToGPU(
            VulkanDevice& device,
            VulkanMemory& allocator,
            const DecodedImageData& data,
            bool generateMipmaps = true,
            std::string_view debugName = "");

        static std::unique_ptr<VulkanTexture> LoadAndUpload(
            VulkanDevice& device,
            VulkanMemory& allocator,
            const std::filesystem::path& path,
            bool srgb = true,
            bool generateMipmaps = true);

        static LoadedTexture LoadAndRegister(
            VulkanDevice& device,
            VulkanMemory& allocator,
            VulkanBindlessHeap& heap,
            const std::filesystem::path& path,
            bool srgb = true,
            bool generateMipmaps = true);
    };
}