#include "VulkanBuffer.h"

#include <cstring>

#include "VulkanDevice.h"
#include "VulkanMemory.h"

namespace Engine
{
    VulkanBuffer::VulkanBuffer(VulkanDevice &device, VulkanMemory &allocator, const BufferDesc &desc): device(device), allocator(allocator)
    {
        size = desc.size;

        VkBufferCreateInfo bufferInfo;
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = static_cast<VkBufferUsageFlags>(desc.usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.size = size;
        bufferInfo.pNext = nullptr;

        VmaAllocationCreateInfo allocInfo{};
        if (desc.memoryUsage == MemoryUsage::GPU_ONLY) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.flags = 0;
        } else if (desc.memoryUsage == MemoryUsage::CPU_TO_GPU) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else if (desc.memoryUsage == MemoryUsage::GPU_TO_CPU) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }
        VmaAllocationInfo resultInfo{};

        VkResult res = vmaCreateBuffer(allocator.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &resultInfo);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to allocate VMA buffer of size {}", size);

        if (resultInfo.pMappedData != nullptr) {
            mappedData = resultInfo.pMappedData;
        }

        if (static_cast<uint32_t>(desc.usage) & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
            bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddressInfo.buffer = buffer;

            gpuAddress = vkGetBufferDeviceAddress(device.GetHandle(), &bufferDeviceAddressInfo);

            ENGINE_ASSERT(gpuAddress != 0, "VulkanBuffer: Failed to retrieve BDA address");
            ENGINE_ASSERT(gpuAddress % 16 == 0, "VulkanBuffer: BDA address must be 16-byte aligned");
        }

        if (!desc.debugName.empty()) {
            device.SetObjectName(buffer, desc.debugName);
        }
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (mappedData != nullptr) {
            vmaUnmapMemory(allocator.GetAllocator(), allocation);
        }

        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator.GetAllocator(), buffer, allocation);
        }

        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        size = 0;
        gpuAddress = 0;
        mappedData = nullptr;
    }

    VulkanBuffer::VulkanBuffer(VulkanBuffer &&other) noexcept
    : device(other.device)
   , allocator(other.allocator)
   , buffer(other.buffer)
   , allocation(other.allocation)
   , size(other.size)
   , gpuAddress(other.gpuAddress)
   , mappedData(other.mappedData)
    {
        other.buffer = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.size = 0;
        other.gpuAddress = 0;
        other.mappedData = nullptr;
    }

    VulkanBuffer & VulkanBuffer::operator=(VulkanBuffer &&other) noexcept
    {
        if (this != &other) {
            if (mappedData != nullptr) {
                vmaUnmapMemory(allocator.GetAllocator(), allocation);
                mappedData = nullptr;
            }
            if (buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator.GetAllocator(), buffer, allocation);
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
            }

            buffer = other.buffer;
            allocation = other.allocation;
            size = other.size;
            gpuAddress = other.gpuAddress;
            mappedData = other.mappedData;

            other.buffer = VK_NULL_HANDLE;
            other.allocation = VK_NULL_HANDLE;
            other.size = 0;
            other.gpuAddress = 0;
            other.mappedData = nullptr;
        }
        return *this;
    }

    void * VulkanBuffer::Map()
    {
        if (mappedData != nullptr) {
            return mappedData;
        }

        VkResult result = vmaMapMemory(allocator.GetAllocator(), allocation, &mappedData);
        ENGINE_VERIFY(result == VK_SUCCESS, "Failed to map buffer memory");
        return mappedData;
    }

    void VulkanBuffer::Unmap()
    {
        vmaUnmapMemory(allocator.GetAllocator(), allocation);
        mappedData = nullptr;
    }

    void VulkanBuffer::UpdateData(const void *data, VkDeviceSize size, VkDeviceSize offset)
    {
        void* mapped = Map();
        ENGINE_ASSERT(mapped != nullptr, "Cannot copy to unmapped buffer");
        VkDeviceSize copySize = (size == VK_WHOLE_SIZE) ? (this->size - offset) : size;
        ENGINE_ASSERT(offset + copySize <= this->size, "UpdateData out of bounds");
        char *memOffset = static_cast<char *>(mapped) + offset;
        std::memcpy(memOffset, data, copySize);
        Flush(copySize, offset);
    }

    void VulkanBuffer::Flush(VkDeviceSize size, VkDeviceSize offset)
    {
       vmaFlushAllocation(allocator.GetAllocator(), allocation, offset, size);
    }
} // Engine