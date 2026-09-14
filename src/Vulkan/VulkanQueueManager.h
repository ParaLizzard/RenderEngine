#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <mutex>

#include "Core/CoreDefines.h"
#include "Vulkan/VulkanDevice.h"

namespace Engine {
    class VulkanDevice;

    struct QueueSubmitDesc {
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
        std::vector<VkSemaphoreSubmitInfo> signalSemaphores;
        VkFence signalFence = VK_NULL_HANDLE;
    };

    class VulkanQueueManager {
    public:
        explicit VulkanQueueManager(VulkanDevice& device);
        ~VulkanQueueManager();

        ENGINE_NODISCARD VkQueue GetQueue(QueueType type) const noexcept;
        ENGINE_NODISCARD uint32_t GetQueueFamilyIndex(QueueType type) const noexcept;

        ENGINE_NODISCARD VkSemaphore GetTimelineSemaphore(QueueType type) const noexcept;
        ENGINE_NODISCARD uint64_t GetCurrentTimelineValue(QueueType type) const noexcept;
        uint64_t IncrementTimelineValue(QueueType type) noexcept;
        void HostWaitTimeline(QueueType type, uint64_t value, uint64_t timeoutNs = UINT64_MAX);

        void Submit(QueueType type, const QueueSubmitDesc& desc);
        void WaitIdle(QueueType type);
        void WaitDeviceIdle();

    private:
        VulkanDevice& device;

        VkSemaphore graphicsTimeline = VK_NULL_HANDLE;
        VkSemaphore computeTimeline = VK_NULL_HANDLE;
        VkSemaphore transferTimeline = VK_NULL_HANDLE;

        uint64_t graphicsTimelineValue = 0;
        uint64_t computeTimelineValue = 0;
        uint64_t transferTimelineValue = 0;

        std::mutex submitMutex;
    };
}
