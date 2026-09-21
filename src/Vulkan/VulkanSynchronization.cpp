#include "Vulkan/VulkanSynchronization.h"
#include <vector>

namespace Engine {

    namespace {
        struct AccessStageFlags {
            VkPipelineStageFlags2 stageMask = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2 accessMask = VK_ACCESS_2_NONE;
        };

        AccessStageFlags GetSrcAccessStage(VkImageLayout layout) {
            switch (layout) {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                    return { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE };

                case VK_IMAGE_LAYOUT_PREINITIALIZED:
                    return { VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT };

                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT };

                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT };

                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE };

                case VK_IMAGE_LAYOUT_GENERAL:
                    return {
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
                    };

                default:
                    return {
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
                    };
            }
        }

        AccessStageFlags GetDstAccessStage(VkImageLayout layout) {
            switch (layout) {
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return {
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT
                    };

                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT };

                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT };

                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return { VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE };

                case VK_IMAGE_LAYOUT_GENERAL:
                    return {
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
                        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
                    };

                default:
                    return {
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
                    };
            }
        }
    } // namespace

    void VulkanSync::PipelineBarrier(VkCommandBuffer cmd,
                                     std::span<const ImageBarrier2> imageBarriers,
                                     std::span<const BufferBarrier2> bufferBarriers)
    {
        std::vector<VkImageMemoryBarrier2> vkImageBarriers;
        vkImageBarriers.reserve(imageBarriers.size());
        for (const auto& b : imageBarriers) {
            VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = b.srcStageMask,
                .srcAccessMask = b.srcAccessMask,
                .dstStageMask = b.dstStageMask,
                .dstAccessMask = b.dstAccessMask,
                .oldLayout = b.oldLayout,
                .newLayout = b.newLayout,
                .srcQueueFamilyIndex = b.srcQueueFamilyIndex,
                .dstQueueFamilyIndex = b.dstQueueFamilyIndex,
                .image = b.image,
                .subresourceRange = b.subresourceRange
            };
            vkImageBarriers.push_back(barrier);
        }

        std::vector<VkBufferMemoryBarrier2> vkBufferBarriers;
        vkBufferBarriers.reserve(bufferBarriers.size());
        for (const auto& b : bufferBarriers) {
            VkBufferMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = b.srcStageMask,
                .srcAccessMask = b.srcAccessMask,
                .dstStageMask = b.dstStageMask,
                .dstAccessMask = b.dstAccessMask,
                .srcQueueFamilyIndex = b.srcQueueFamilyIndex,
                .dstQueueFamilyIndex = b.dstQueueFamilyIndex,
                .buffer = b.buffer,
                .offset = b.offset,
                .size = b.size
            };
            vkBufferBarriers.push_back(barrier);
        }

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(vkBufferBarriers.size()),
            .pBufferMemoryBarriers = vkBufferBarriers.empty() ? nullptr : vkBufferBarriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(vkImageBarriers.size()),
            .pImageMemoryBarriers = vkImageBarriers.empty() ? nullptr : vkImageBarriers.data()
        };

        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    void VulkanSync::TransitionLayout(VkCommandBuffer cmd,
                                      VkImage image,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      VkImageAspectFlags aspectMask,
                                      uint32_t mipLevels,
                                      uint32_t arrayLayers)
    {
        AccessStageFlags src = GetSrcAccessStage(oldLayout);
        AccessStageFlags dst = GetDstAccessStage(newLayout);

        ImageBarrier2 barrier{
            .image = image,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcStageMask = src.stageMask,
            .srcAccessMask = src.accessMask,
            .dstStageMask = dst.stageMask,
            .dstAccessMask = dst.accessMask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = arrayLayers
            }
        };

        PipelineBarrier(cmd, std::span<const ImageBarrier2>(&barrier, 1));
    }

} // namespace Engine
