#include "Vulkan/VulkanTexture.h"

#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanSynchronization.h"
#include "Vulkan/VkUtils.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <format>
#include <algorithm>

namespace Engine {

    VulkanTexture::VulkanTexture(VulkanDevice& device, VulkanMemory& allocator, const TextureDesc& inDesc)
        : device(device), allocator(&allocator), desc(inDesc), ownsImage(true)
    {
        desc.mipLevels = std::max(1u, desc.mipLevels);
        desc.arrayLayers = std::max(1u, desc.arrayLayers);
        desc.extent.depth = std::max(1u, desc.extent.depth);

        if ((desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE || desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) && desc.arrayLayers < 6) {
            desc.arrayLayers = 6;
        }

        VkImageCreateFlags imageFlags = 0;
        if (desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE || desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
            imageFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
        if (desc.imageType == VK_IMAGE_TYPE_3D && desc.viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
            imageFlags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
        }

        VkImageUsageFlags usageFlags = static_cast<VkImageUsageFlags>(desc.usage);
        if (desc.mipLevels > 1) {
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = imageFlags,
            .imageType = desc.imageType,
            .format = desc.format,
            .extent = desc.extent,
            .mipLevels = desc.mipLevels,
            .arrayLayers = desc.arrayLayers,
            .samples = desc.sampleCount,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VkResult res = vmaCreateImage(allocator.GetAllocator(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to allocate GPU image for '{}'", desc.debugName);

        if (!desc.debugName.empty()) {
            device.SetObjectName(image, desc.debugName);
        }

        CreateDefaultView();
    }

    VulkanTexture::VulkanTexture(VulkanDevice& device, VkImage existingImage, const TextureDesc& inDesc)
        : device(device), allocator(nullptr), desc(inDesc), image(existingImage), allocation(VK_NULL_HANDLE), ownsImage(false)
    {
        desc.mipLevels = std::max(1u, desc.mipLevels);
        desc.arrayLayers = std::max(1u, desc.arrayLayers);
        desc.extent.depth = std::max(1u, desc.extent.depth);

        ENGINE_ASSERT(image != VK_NULL_HANDLE, "existingImage must not be VK_NULL_HANDLE");

        if (!desc.debugName.empty()) {
            device.SetObjectName(image, desc.debugName);
        }

        CreateDefaultView();
    }

    VulkanTexture::~VulkanTexture()
    {
        for (VkImageView view : mipViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device.GetHandle(), view, nullptr);
            }
        }
        mipViews.clear();

        if (defaultView != VK_NULL_HANDLE) {
            vkDestroyImageView(device.GetHandle(), defaultView, nullptr);
            defaultView = VK_NULL_HANDLE;
        }

        if (ownsImage && image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE && allocator != nullptr) {
            vmaDestroyImage(allocator->GetAllocator(), image, allocation);
        }

        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    VulkanTexture::VulkanTexture(VulkanTexture&& other) noexcept
        : device(other.device)
        , allocator(other.allocator)
        , desc(other.desc)
        , image(other.image)
        , allocation(other.allocation)
        , defaultView(other.defaultView)
        , mipViews(std::move(other.mipViews))
        , ownsImage(other.ownsImage)
        , bindlessSlot(other.bindlessSlot)
    {
        other.image = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.defaultView = VK_NULL_HANDLE;
        other.ownsImage = false;
        other.allocator = nullptr;
        other.bindlessSlot = 0xFFFFFFFF;
    }

    VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) noexcept
    {
        if (this != &other) {
            for (VkImageView view : mipViews) {
                if (view != VK_NULL_HANDLE) {
                    vkDestroyImageView(device.GetHandle(), view, nullptr);
                }
            }
            mipViews.clear();

            if (defaultView != VK_NULL_HANDLE) {
                vkDestroyImageView(device.GetHandle(), defaultView, nullptr);
                defaultView = VK_NULL_HANDLE;
            }

            if (ownsImage && image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE && allocator != nullptr) {
                vmaDestroyImage(allocator->GetAllocator(), image, allocation);
            }

            allocator = other.allocator;
            desc = other.desc;
            image = other.image;
            allocation = other.allocation;
            defaultView = other.defaultView;
            mipViews = std::move(other.mipViews);
            ownsImage = other.ownsImage;
            bindlessSlot = other.bindlessSlot;

            other.image = VK_NULL_HANDLE;
            other.allocation = VK_NULL_HANDLE;
            other.defaultView = VK_NULL_HANDLE;
            other.ownsImage = false;
            other.allocator = nullptr;
            other.bindlessSlot = 0xFFFFFFFF;
        }
        return *this;
    }

    VkImageAspectFlags VulkanTexture::DeduceAspectMask() const noexcept
    {
        if (desc.format == VK_FORMAT_S8_UINT) {
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        if (VkUtils::isDepthFormat(desc.format)) {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    void VulkanTexture::CreateDefaultView()
    {
        VkImageAspectFlags aspectMask = DeduceAspectMask();

        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = desc.viewType,
            .format = desc.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = desc.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = desc.arrayLayers
            }
        };

        VkResult res = vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &defaultView);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create default image view for '{}'", desc.debugName);

        if (!desc.debugName.empty()) {
            std::string viewName = std::format("{}_View", desc.debugName);
            device.SetObjectName(defaultView, viewName);
        }
    }

    VkImageView VulkanTexture::GetMipView(uint32_t mipLevel) const
    {
        ENGINE_ASSERT(mipLevel < desc.mipLevels, "Requested mip level {} exceeds total levels {}", mipLevel, desc.mipLevels);

        if (mipViews.size() < desc.mipLevels) {
            mipViews.resize(desc.mipLevels, VK_NULL_HANDLE);
        }

        if (mipViews[mipLevel] == VK_NULL_HANDLE) {
            VkImageViewType mipViewType = desc.viewType;
            if (desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE || desc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
                mipViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }

            VkImageViewCreateInfo viewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = image,
                .viewType = mipViewType,
                .format = desc.format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = {
                    .aspectMask = DeduceAspectMask(),
                    .baseMipLevel = mipLevel,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = desc.arrayLayers
                }
            };

            VkResult res = vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &mipViews[mipLevel]);
            ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create mip view {} for '{}'", mipLevel, desc.debugName);

            if (!desc.debugName.empty()) {
                std::string viewName = std::format("{}_MipView{}", desc.debugName, mipLevel);
                device.SetObjectName(mipViews[mipLevel], viewName);
            }
        }

        return mipViews[mipLevel];
    }

    void VulkanTexture::GenerateMipmaps(VkCommandBuffer cmd)
    {
        if (desc.mipLevels <= 1) {
            return;
        }

        VkFormatProperties formatProps = device.GetFormatProperties(desc.format);
        ENGINE_ASSERT((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                      (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) &&
                      (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
                      "Format does not support linear blitting for mipmap generation");

        int32_t mipWidth = static_cast<int32_t>(desc.extent.width);
        int32_t mipHeight = static_cast<int32_t>(desc.extent.height);
        int32_t mipDepth = static_cast<int32_t>(desc.extent.depth);
        VkImageAspectFlags aspect = DeduceAspectMask();

        for (uint32_t i = 1; i < desc.mipLevels; ++i) {
            ImageBarrier2 srcBarrier{
                .image = image,
                .oldLayout = (i == 1) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = i - 1,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = desc.arrayLayers
                }
            };
            VulkanSync::PipelineBarrier(cmd, std::span<const ImageBarrier2>(&srcBarrier, 1));

            ImageBarrier2 dstBarrier{
                .image = image,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = i,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = desc.arrayLayers
                }
            };
            VulkanSync::PipelineBarrier(cmd, std::span<const ImageBarrier2>(&dstBarrier, 1));

            int32_t nextWidth = std::max(1, mipWidth / 2);
            int32_t nextHeight = std::max(1, mipHeight / 2);
            int32_t nextDepth = std::max(1, mipDepth / 2);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
            blit.srcSubresource.aspectMask = aspect;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = desc.arrayLayers;

            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { nextWidth, nextHeight, nextDepth };
            blit.dstSubresource.aspectMask = aspect;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = desc.arrayLayers;

            vkCmdBlitImage(cmd,
                           image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR);

            ImageBarrier2 readBarrier{
                .image = image,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = i - 1,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = desc.arrayLayers
                }
            };
            VulkanSync::PipelineBarrier(cmd, std::span<const ImageBarrier2>(&readBarrier, 1));

            mipWidth = nextWidth;
            mipHeight = nextHeight;
            mipDepth = nextDepth;
        }

        ImageBarrier2 lastMipBarrier{
            .image = image,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = desc.mipLevels - 1,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = desc.arrayLayers
            }
        };
        VulkanSync::PipelineBarrier(cmd, std::span<const ImageBarrier2>(&lastMipBarrier, 1));
    }

} // namespace Engine
