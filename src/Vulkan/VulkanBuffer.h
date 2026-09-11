#pragma once
#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include <string_view>
#include <span>

#include "Core/CoreDefines.h"

namespace Engine {
    class VulkanDevice;
    class VulkanMemory;

    enum class BufferUsage : uint32_t
    {
        VERTEX = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        INDEX = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        UNIFORM = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        STORAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        INDIRECT = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        STAGING = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TRANSFER_DST = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };

    inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class MemoryUsage {
        GPU_ONLY,
        CPU_TO_GPU,
        GPU_TO_CPU
    };

    struct BufferDesc {
        std::string_view debugName;
        VkDeviceSize size = 0;
        BufferUsage usage = BufferUsage::STORAGE;
        MemoryUsage memoryUsage = MemoryUsage::GPU_ONLY;
    };

    class VulkanBuffer {
    public:
        VulkanBuffer(VulkanDevice& device, VulkanMemory& allocator, const BufferDesc& desc);
        ~VulkanBuffer();

        ENGINE_NON_COPYABLE(VulkanBuffer);

        VulkanBuffer(VulkanBuffer&& other) noexcept;
        VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

        ENGINE_NODISCARD VkBuffer GetHandle() const noexcept { return buffer; }
        ENGINE_NODISCARD VkDeviceSize GetSize() const noexcept { return size; }
        ENGINE_NODISCARD VkDeviceAddress GetDeviceAddress() const noexcept { return gpuAddress; }

        void* Map();
        void Unmap();
        void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
        void Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    private:
        VulkanDevice& device;
        VulkanMemory& allocator;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkDeviceAddress gpuAddress = 0;
        void* mappedData = nullptr;
    };
}
