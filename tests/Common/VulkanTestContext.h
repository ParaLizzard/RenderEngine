#pragma once

#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <memory>

namespace Engine::Test {

    class VulkanTestContext {
    public:
        static VulkanTestContext& Get();

        VulkanTestContext();
        ~VulkanTestContext();

        // Non-copyable, non-movable
        VulkanTestContext(const VulkanTestContext&) = delete;
        VulkanTestContext& operator=(const VulkanTestContext&) = delete;

        bool Initialize();
        void Shutdown();

        bool IsInitialized() const noexcept { return initialized; }

        VkInstance GetInstance() const noexcept { return instance; }
        VkPhysicalDevice GetPhysicalDevice() const noexcept { return physicalDevice; }
        VkDevice GetDevice() const noexcept { return device; }
        VkQueue GetGraphicsQueue() const noexcept { return graphicsQueue; }
        VkQueue GetComputeQueue() const noexcept { return computeQueue; }
        uint32_t GetGraphicsQueueFamily() const noexcept { return graphicsFamily; }
        uint32_t GetComputeQueueFamily() const noexcept { return computeFamily; }
        VmaAllocator GetAllocator() const noexcept { return allocator; }

        void WaitIdle() const;

        bool SupportsExtension(const char* extensionName) const;
        bool SupportsBufferDeviceAddress() const noexcept { return bdaSupported; }
        bool SupportsMeshShaders() const noexcept { return meshShaderSupported; }
        bool SupportsSynchronization2() const noexcept { return sync2Supported; }
        bool SupportsDynamicRendering() const noexcept { return dynamicRenderingSupported; }

        const VkPhysicalDeviceProperties& GetProperties() const noexcept { return properties; }
        const VkPhysicalDeviceFeatures2& GetFeatures() const noexcept { return features2; }

    private:
        bool createInstance();
        bool pickPhysicalDevice();
        bool createLogicalDevice();
        bool createAllocator();

        bool initialized = false;
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue computeQueue = VK_NULL_HANDLE;
        uint32_t graphicsFamily = ~0u;
        uint32_t computeFamily = ~0u;
        VmaAllocator allocator = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures2 features2{};
        VkPhysicalDeviceVulkan11Features features11{};
        VkPhysicalDeviceVulkan12Features features12{};
        VkPhysicalDeviceVulkan13Features features13{};
        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};

        std::vector<VkExtensionProperties> availableExtensions;
        bool bdaSupported = false;
        bool meshShaderSupported = false;
        bool sync2Supported = false;
        bool dynamicRenderingSupported = false;
    };

    // GoogleTest fixture that ensures headless Vulkan context is ready
    class HeadlessVulkanTest : public ::testing::Test {
    protected:
        void SetUp() override {
            context = &VulkanTestContext::Get();
            if (!context->IsInitialized()) {
                if (!context->Initialize()) {
                    GTEST_SKIP() << "VulkanTestContext failed to initialize headless Vulkan device on this system.";
                }
            }
        }

        VulkanTestContext* context = nullptr;
    };

} // namespace Engine::Test
