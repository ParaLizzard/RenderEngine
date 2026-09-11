/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

#include "Vulkan/Buffer.h"

#include <cstring>
#include <vulkan/vulkan.h>

#include "Core/Assert.h"
#include "Vulkan/VulkanDevice.h"

namespace Engine {
    Buffer::Buffer(VulkanDevice &device,
                   VkDeviceSize instanceSize,
                   uint32_t instanceCount,
                   VkBufferUsageFlags usageFlags,
                   VmaMemoryUsage memoryUsage,
                   VkMemoryPropertyFlags memoryPropertyFlags,
                   VkDeviceSize minOffsetAlignment):
        device(device), instanceCount(instanceCount), instanceSize(instanceSize), usageFlags(usageFlags),
        memoryPropertyFlags(memoryPropertyFlags)
    {
        alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
        bufferSize = alignmentSize * instanceCount;
        VmaAllocationInfo allocationInfo = {};
        createBuffer(bufferSize, usageFlags, memoryUsage, buffer, allocation, &allocationInfo);
        mapped = allocationInfo.pMappedData;
    }

    Buffer::~Buffer()
    {
        vmaDestroyBuffer(device.getAllocator(), buffer, allocation);
    }

    void Buffer::createBuffer(VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memUsage,
                              VkBuffer &buffer,
                              VmaAllocation &allocation,
                              VmaAllocationInfo *pResultInfo)
    {
        ENGINE_VERIFY(size > 0, "Buffers: Attempted to create a buffer of size 0");

        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = memUsage;


        if (memUsage == VMA_MEMORY_USAGE_CPU_TO_GPU || memUsage == VMA_MEMORY_USAGE_CPU_ONLY) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else if (memUsage == VMA_MEMORY_USAGE_GPU_ONLY) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.flags = 0;
        } else {
            allocInfo.usage = memUsage;
            allocInfo.flags = 0;
        }

        VkResult result =
            vmaCreateBuffer(device.getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, pResultInfo);

        if (result != VK_SUCCESS && (memUsage == VMA_MEMORY_USAGE_CPU_TO_GPU)) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            result = vmaCreateBuffer(device.getAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, pResultInfo);
        }

        ENGINE_VERIFY(result == VK_SUCCESS, "Buffers: failed to create VMA buffer");
    }

    void Buffer::copyBuffer(VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = device.BeginSingleTimeCommands(QueueType::Graphics);

        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, buffer, dstBuffer, 1, &copyRegion);

        device.EndSingleTimeCommands(commandBuffer, QueueType::Graphics);
    }

    void Buffer::copyBufferToImage(VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
    {
        VkCommandBuffer commandBuffer = device.BeginSingleTimeCommands(QueueType::Graphics);

        VkBufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = layerCount;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        device.EndSingleTimeCommands(commandBuffer, QueueType::Graphics);
    }

    VkDeviceSize Buffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
    {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    void Buffer::writeToBuffer(const void *data, VkDeviceSize size, VkDeviceSize offset)
    {
        ENGINE_ASSERT(mapped != nullptr, "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            char *memOffset = (char *)mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }

    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset)
    {
        return vmaFlushAllocation(device.getAllocator(), allocation, offset, size);
    }

    VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
    {
        return vmaInvalidateAllocation(device.getAllocator(), allocation, offset, size);
    }

    VkDescriptorBufferInfo Buffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset)
    {
        return VkDescriptorBufferInfo {
            buffer,
            offset,
            size,
        };
    }

    void Buffer::writeToIndex(void *data, int index)
    {
        writeToBuffer(data, instanceSize, index * alignmentSize);
    }

    VkResult Buffer::flushIndex(int index)
    {
        return flush(alignmentSize, index * alignmentSize);
    }

    VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(int index)
    {
        return descriptorInfo(alignmentSize, index * alignmentSize);
    }

    VkResult Buffer::invalidateIndex(int index)
    {
        return invalidate(alignmentSize, index * alignmentSize);
    }
} // namespace Engine
