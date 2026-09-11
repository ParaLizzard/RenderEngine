#pragma once
#include <vulkan/vulkan.h>
#include "Core/CoreDefines.h"
#include "vma/vk_mem_alloc.h"

namespace Engine {
    class VulkanDevice;

    struct VRAMBudget {
        VkDeviceSize totalHeapBudget = 0;
        VkDeviceSize totalHeapUsage = 0;
        VkDeviceSize availableMemory = 0;
    };

    class VulkanMemory {
    public:
        explicit VulkanMemory(VulkanDevice& device);
        ~VulkanMemory();

        ENGINE_NON_COPYABLE(VulkanMemory);

        VulkanMemory(VulkanMemory&& other) noexcept;
        VulkanMemory& operator=(VulkanMemory&& other) noexcept;

        ENGINE_NODISCARD VmaAllocator GetAllocator() const noexcept { return allocator; }

        ENGINE_NODISCARD VRAMBudget GetVRAMBudget() const;

    private:
        VulkanDevice& device;
        VmaAllocator allocator = VK_NULL_HANDLE;
    };
}