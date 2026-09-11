#define VMA_IMPLEMENTATION
#include "VulkanMemory.h"
#include "VulkanDevice.h"
#include "Core/Log.h"
#include "Core/Assert.h"

namespace Engine
{
    VulkanMemory::VulkanMemory(VulkanDevice &device): device(device)
    {
        VmaVulkanFunctions vulkanFunctions {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo {};
        allocatorInfo.device = device.GetHandle();
        allocatorInfo.physicalDevice = device.GetPhysicalDevice();
        allocatorInfo.instance = device.GetInstance();
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;
        allocatorInfo.flags = 0;

        if (device.GetCapabilities().supportsMemoryBudget) {
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }

        if (device.GetCapabilities().supportsBufferDeviceAddress) {
            allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        ENGINE_VERIFY(vmaCreateAllocator(&allocatorInfo, &allocator) == VK_SUCCESS,
          "Failed to create allocator");
    }

    VulkanMemory::~VulkanMemory()
    {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
    }

    VulkanMemory::VulkanMemory(VulkanMemory&& other) noexcept
        : device(other.device),
          allocator(other.allocator)
    {
        other.allocator = VK_NULL_HANDLE;
    }

    VulkanMemory& VulkanMemory::operator=(VulkanMemory&& other) noexcept
    {
        if (this != &other) {
            if (allocator != VK_NULL_HANDLE) {
                vmaDestroyAllocator(allocator);
            }
            allocator = other.allocator;
            other.allocator = VK_NULL_HANDLE;
        }
        return *this;
    }

    VRAMBudget VulkanMemory::GetVRAMBudget() const
    {
        VRAMBudget result{};

        if (allocator == VK_NULL_HANDLE) {
            return result;
        }

        VmaBudget heapBudgets[VK_MAX_MEMORY_HEAPS]{};
        vmaGetHeapBudgets(allocator, heapBudgets);

        const VkPhysicalDeviceMemoryProperties& memProps =
        device.GetMemoryProperties().memoryProperties;

        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                result.totalHeapBudget += heapBudgets[i].budget;
                result.totalHeapUsage  += heapBudgets[i].usage;
            }
        }

        if (result.totalHeapBudget > result.totalHeapUsage) {
            result.availableMemory = result.totalHeapBudget - result.totalHeapUsage;
        } else {
            result.availableMemory = 0;
        }

        return result;
    }
} // Engine