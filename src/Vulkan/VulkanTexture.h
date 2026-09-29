#pragma once

#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include <string_view>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <bit>

#include "Core/CoreDefines.h"

namespace Engine {
    class VulkanDevice;
    class VulkanMemory;

    enum class TextureUsage : uint32_t {
        Sampled         = VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        DepthAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        TransferSrc     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDst     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    };

    inline constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) noexcept {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) noexcept {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline TextureUsage& operator|=(TextureUsage& a, TextureUsage b) noexcept {
        a = a | b;
        return a;
    }

    inline TextureUsage& operator&=(TextureUsage& a, TextureUsage b) noexcept {
        a = a & b;
        return a;
    }

    inline constexpr bool HasUsage(TextureUsage flags, TextureUsage flag) noexcept {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }

    struct TextureDesc {
        std::string_view debugName;
        VkExtent3D extent{ 0, 0, 1 };
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        TextureUsage usage = TextureUsage::Sampled;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        VkImageType imageType = VK_IMAGE_TYPE_2D;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    };

    class VulkanTexture {
    public:
        VulkanTexture(VulkanDevice& device, VulkanMemory& allocator, const TextureDesc& desc);
        VulkanTexture(VulkanDevice& device, VkImage existingImage, const TextureDesc& desc);
        ~VulkanTexture();

        ENGINE_NON_COPYABLE(VulkanTexture);
        VulkanTexture(VulkanTexture&& other) noexcept;
        VulkanTexture& operator=(VulkanTexture&& other) noexcept;

        ENGINE_NODISCARD VkImage GetHandle() const noexcept { return image; }
        ENGINE_NODISCARD VkImageView GetImageView() const noexcept { return defaultView; }
        ENGINE_NODISCARD VkImageView GetMipView(uint32_t mipLevel) const;

        ENGINE_NODISCARD VkFormat GetFormat() const noexcept { return desc.format; }
        ENGINE_NODISCARD VkExtent3D GetExtent() const noexcept { return desc.extent; }
        ENGINE_NODISCARD VkExtent2D GetExtent2D() const noexcept { return { desc.extent.width, desc.extent.height }; }
        ENGINE_NODISCARD uint32_t GetMipLevels() const noexcept { return desc.mipLevels; }
        ENGINE_NODISCARD uint32_t GetArrayLayers() const noexcept { return desc.arrayLayers; }
        ENGINE_NODISCARD const TextureDesc& GetDesc() const noexcept { return desc; }
        ENGINE_NODISCARD bool OwnsImage() const noexcept { return ownsImage; }
        ENGINE_NODISCARD uint32_t GetBindlessSlot() const noexcept { return bindlessSlot; }
        void SetBindlessSlot(uint32_t slot) noexcept { bindlessSlot = slot; }

        void GenerateMipmaps(VkCommandBuffer cmd);

        static constexpr uint32_t CalculateMipLevels(uint32_t width, uint32_t height) noexcept {
            uint32_t maxDim = std::max(width, height);
            if (maxDim == 0) return 1;
            return static_cast<uint32_t>(std::bit_width(maxDim));
        }

    private:
        void CreateDefaultView();
        VkImageAspectFlags DeduceAspectMask() const noexcept;

        VulkanDevice& device;
        VulkanMemory* allocator = nullptr;
        TextureDesc desc;
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView defaultView = VK_NULL_HANDLE;
        mutable std::vector<VkImageView> mipViews;
        bool ownsImage = true;
        uint32_t bindlessSlot = 0xFFFFFFFF;
    };
}
