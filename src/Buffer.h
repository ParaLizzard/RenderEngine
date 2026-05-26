#pragma once
#include "Device.h"

namespace Engine
{
    class Buffer
    {
    public:
        Buffer(
            Device& device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VmaMemoryUsage memoryUsage,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment);
        ~Buffer();

        Buffer(Buffer const&) = delete;
        Buffer& operator=(Buffer const&) = delete;

        VkBuffer getBuffer() { return buffer; }
        void* getMappedMemory() { return mapped; }
        uint32_t getInstanceCount(){ return instanceCount; }
        VkDeviceSize getInstanceSize() { return instanceSize; }
        VkDeviceSize getAlignmentSize() { return alignmentSize; }
        VkBufferUsageFlags getUsageFlags() { return usageFlags; }
        VkMemoryPropertyFlags getMemoryPropertyFlags(){ return memoryPropertyFlags; }
        VkDeviceSize getBufferSize(){ return bufferSize; }

        VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
        void writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset);
        VkResult flush(VkDeviceSize size, VkDeviceSize offset);
        VkResult invalidate(VkDeviceSize size, VkDeviceSize offset);
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size, VkDeviceSize offset);
        void writeToIndex(void* data, int index);
        VkResult flushIndex(int index);
        VkDescriptorBufferInfo descriptorInfoForIndex(int index);
        VkResult invalidateIndex(int index);

    private:
        Device& device;
        uint32_t instanceCount;
        VkDeviceSize instanceSize;
        VkDeviceSize alignmentSize;
        VkDeviceSize bufferSize;
        VkBufferUsageFlags usageFlags;
        VkMemoryPropertyFlags memoryPropertyFlags;
        VmaAllocation allocation;

        void* mapped = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };
}
