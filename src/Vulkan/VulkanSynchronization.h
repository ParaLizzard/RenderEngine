#pragma once
#include <vulkan/vulkan.h>
#include <span>

namespace Engine {
    struct ImageBarrier2 {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        VkImageSubresourceRange subresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
    };

    struct BufferBarrier2 {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize size = VK_WHOLE_SIZE;
        VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    };

    class VulkanSync {
    public:
        static void PipelineBarrier(VkCommandBuffer cmd,
                                    std::span<const ImageBarrier2> imageBarriers,
                                    std::span<const BufferBarrier2> bufferBarriers = {});

        static void TransitionLayout(VkCommandBuffer cmd,
                                     VkImage image,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     uint32_t mipLevels = VK_REMAINING_MIP_LEVELS,
                                     uint32_t arrayLayers = VK_REMAINING_ARRAY_LAYERS);
    };
}