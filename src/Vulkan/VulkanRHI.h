#pragma once
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <memory>
#include <vulkan/vulkan.h>
#include "Core/ISubsystem.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanBindlessHeap.h"
#include "Vulkan/VulkanPipelineCache.h"
#include "Vulkan/VulkanQueueManager.h"
#include "Vulkan/VulkanSwapchain.h"

namespace Engine {
    class WindowSubsystem;

    class VulkanRHI : public ISubsystem {
    public:
        VulkanRHI() = default;
        ~VulkanRHI() override = default;

        ENGINE_NODISCARD std::string_view GetName() const override { return "VulkanRHI"; }
        bool Initialize(SubsystemRegistry& registry) override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        ENGINE_NODISCARD VulkanDevice& GetDevice() noexcept { return *device; }
        ENGINE_NODISCARD const VulkanDevice& GetDevice() const noexcept { return *device; }

        ENGINE_NODISCARD VulkanMemory& GetMemory() noexcept { return *memory; }
        ENGINE_NODISCARD const VulkanMemory& GetMemory() const noexcept { return *memory; }

        ENGINE_NODISCARD VulkanBindlessHeap& GetBindlessHeap() noexcept { return *bindlessHeap; }
        ENGINE_NODISCARD const VulkanBindlessHeap& GetBindlessHeap() const noexcept { return *bindlessHeap; }

        ENGINE_NODISCARD VulkanPipelineCache& GetPipelineCache() noexcept { return *pipelineCache; }
        ENGINE_NODISCARD const VulkanPipelineCache& GetPipelineCache() const noexcept { return *pipelineCache; }

        ENGINE_NODISCARD VulkanQueueManager& GetQueueManager() noexcept { return *queueManager; }
        ENGINE_NODISCARD const VulkanQueueManager& GetQueueManager() const noexcept { return *queueManager; }

        ENGINE_NODISCARD VulkanSwapchain* GetSwapchain() noexcept { return swapchain.get(); }
        ENGINE_NODISCARD const VulkanSwapchain* GetSwapchain() const noexcept { return swapchain.get(); }

        ENGINE_NODISCARD VkInstance GetInstance() const noexcept { return instance; }
        ENGINE_NODISCARD VkSurfaceKHR GetSurface() const noexcept { return surface; }

    private:
        bool CreateInstance(bool enableValidation, bool hasWindow);
        bool SetupDebugMessenger();

        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        std::unique_ptr<VulkanDevice> device;
        std::unique_ptr<VulkanMemory> memory;
        std::unique_ptr<VulkanBindlessHeap> bindlessHeap;
        std::unique_ptr<VulkanPipelineCache> pipelineCache;
        std::unique_ptr<VulkanQueueManager> queueManager;
        std::unique_ptr<VulkanSwapchain> swapchain;
    };
}
