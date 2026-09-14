#include "VulkanQueueManager.h"


namespace Engine
{
    VulkanQueueManager::VulkanQueueManager(VulkanDevice &device) : device(device)
    {
        VkSemaphoreTypeCreateInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineInfo.initialValue = 0;

        VkSemaphoreCreateInfo semCreateInfo{};
        semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semCreateInfo.pNext = &timelineInfo;
        semCreateInfo.flags = 0;

        VkResult result;
        result = vkCreateSemaphore(device.GetHandle(), &semCreateInfo, nullptr, &graphicsTimeline);
        ENGINE_VERIFY(result == VK_SUCCESS, "Failed to create graphics timeline semaphore!");

        result = vkCreateSemaphore(device.GetHandle(), &semCreateInfo, nullptr, &computeTimeline);
        ENGINE_VERIFY(result == VK_SUCCESS, "Failed to create compute timeline semaphore!");

        result = vkCreateSemaphore(device.GetHandle(), &semCreateInfo, nullptr, &transferTimeline);
        ENGINE_VERIFY(result == VK_SUCCESS, "Failed to create transfer timeline semaphore!");

        device.SetObjectName(graphicsTimeline, "Graphics_TimelineSemaphore");
        device.SetObjectName(computeTimeline, "Compute_TimelineSemaphore");
        device.SetObjectName(transferTimeline, "Transfer_TimelineSemaphore");
    }


    VulkanQueueManager::~VulkanQueueManager()
    {
        if (graphicsTimeline != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.GetHandle(), graphicsTimeline, nullptr);
        }
        if (computeTimeline != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.GetHandle(), computeTimeline, nullptr);
        }
        if (transferTimeline != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.GetHandle(), transferTimeline, nullptr);
        }
    }

    VkQueue VulkanQueueManager::GetQueue(QueueType type) const noexcept
    {
        return device.GetQueue(type);
    }

    uint32_t VulkanQueueManager::GetQueueFamilyIndex(QueueType type) const noexcept
    {
        return device.GetQueueFamily(type);
    }

    VkSemaphore VulkanQueueManager::GetTimelineSemaphore(QueueType type) const noexcept
    {
        switch (type) {
            case QueueType::Graphics: {
                return graphicsTimeline;
            }
            case QueueType::Compute: {
                return computeTimeline;
            }
            case QueueType::Transfer: {
                return transferTimeline;
            }
            default: return VK_NULL_HANDLE;
        }
    }

    uint64_t VulkanQueueManager::GetCurrentTimelineValue(QueueType type) const noexcept
    {
        VkSemaphore sem = GetTimelineSemaphore(type);
        if (sem == VK_NULL_HANDLE) {
            return 0;
        }
        uint64_t gpuValue = 0;
        if (vkGetSemaphoreCounterValue(device.GetHandle(), sem, &gpuValue) == VK_SUCCESS) {
            return gpuValue;
        }
        return 0;
    }

    uint64_t VulkanQueueManager::IncrementTimelineValue(QueueType type) noexcept
    {
        switch (type) {
        case QueueType::Graphics: {
            return ++graphicsTimelineValue;
        }
        case QueueType::Compute: {
            return ++computeTimelineValue;
        }
        case QueueType::Transfer: {
            return ++transferTimelineValue;
        }
        default: return 0;
        }
    }

    void VulkanQueueManager::HostWaitTimeline(QueueType type, uint64_t value, uint64_t timeoutNs)
    {
        VkSemaphore sem = GetTimelineSemaphore(type);
        if (sem == VK_NULL_HANDLE) {
            return;
        }

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &sem;
        waitInfo.pValues = &value;
        waitInfo.flags = 0;

        vkWaitSemaphores(device.GetHandle(), &waitInfo, timeoutNs);
    }

    void VulkanQueueManager::Submit(QueueType type, const QueueSubmitDesc &desc)
    {
        std::lock_guard<std::mutex> lock(device.GetQueueMutex(type));

        VkSemaphore timelineSem = GetTimelineSemaphore(type);
        uint64_t newTimelineValue = 0;
        if (timelineSem != VK_NULL_HANDLE) {
            newTimelineValue = IncrementTimelineValue(type);
        }

        std::vector<VkSemaphoreSubmitInfo> signalSemaphores = desc.signalSemaphores;
        if (timelineSem != VK_NULL_HANDLE) {
            VkSemaphoreSubmitInfo timelineSignal{};
            timelineSignal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            timelineSignal.semaphore = timelineSem;
            timelineSignal.value = newTimelineValue;
            timelineSignal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            signalSemaphores.push_back(timelineSignal);
        }

        std::vector<VkCommandBufferSubmitInfo> cmdBufferInfos;
        cmdBufferInfos.reserve(desc.commandBuffers.size());
        for (VkCommandBuffer cmd : desc.commandBuffers) {
            VkCommandBufferSubmitInfo cmdInfo{};
            cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            cmdInfo.commandBuffer = cmd;
            cmdInfo.deviceMask = 0;
            cmdBufferInfos.push_back(cmdInfo);
        }

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(desc.waitSemaphores.size());
        submitInfo.pWaitSemaphoreInfos = desc.waitSemaphores.empty() ? nullptr : desc.waitSemaphores.data();
        submitInfo.commandBufferInfoCount = static_cast<uint32_t>(cmdBufferInfos.size());
        submitInfo.pCommandBufferInfos = cmdBufferInfos.empty() ? nullptr : cmdBufferInfos.data();
        submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphoreInfos = signalSemaphores.empty() ? nullptr : signalSemaphores.data();

        VkResult result = vkQueueSubmit2(device.GetQueue(type), 1, &submitInfo, desc.signalFence);
        ENGINE_VERIFY(result == VK_SUCCESS, "vkQueueSubmit2 failed on queue");
    }

    void VulkanQueueManager::WaitIdle(QueueType type)
    {
        device.WaitQueueIdle(type);
    }

    void VulkanQueueManager::WaitDeviceIdle()
    {
        device.WaitIdle();
    }
} // Engine