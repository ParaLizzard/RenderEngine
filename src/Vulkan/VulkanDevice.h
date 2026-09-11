#pragma once

#include <vulkan/vulkan.h>
#include <string_view>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <functional>
#include <mutex>
#include <cstdint>

#include "Core/CoreDefines.h"
#include "Core/Assert.h"

namespace Engine {

    enum class QueueType : uint8_t
    {
        Graphics = 0,
        Compute,
        Transfer,
        Present,
        Count
    };

    struct QueueFamilyIndices
    {
        uint32_t graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t computeFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t transferFamily = VK_QUEUE_FAMILY_IGNORED;
        uint32_t presentFamily = VK_QUEUE_FAMILY_IGNORED;

        ENGINE_NODISCARD bool IsComplete() const noexcept
        {
            return graphicsFamily != VK_QUEUE_FAMILY_IGNORED &&
                computeFamily != VK_QUEUE_FAMILY_IGNORED &&
                transferFamily != VK_QUEUE_FAMILY_IGNORED &&
                presentFamily != VK_QUEUE_FAMILY_IGNORED;
        }

        ENGINE_NODISCARD bool IsComputeDedicated() const noexcept
        {
            return computeFamily != graphicsFamily;
        }

        ENGINE_NODISCARD bool IsTransferDedicated() const noexcept
        {
            return transferFamily != graphicsFamily && transferFamily != computeFamily;
        }

        ENGINE_NODISCARD bool IsTransferAsync() const noexcept
        {
            return transferFamily != graphicsFamily;
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;

        ENGINE_NODISCARD bool IsAdequate() const noexcept
        {
            return !formats.empty() && !presentModes.empty();
        }
    };

    struct DeviceCapabilities
    {
        bool supportsDynamicRendering = false;
        bool supportsSynchronization2 = false;
        bool supportsMaintenance4 = false;
        bool supportsBufferDeviceAddress = false;
        bool supportsDescriptorIndexing = false;
        bool supportsTimelineSemaphores = false;

        bool supportsMeshShaders = false;
        bool supportsTaskShaders = false;
        bool supportsMemoryBudget = false;
        bool supportsCalibratedTimestamps = false;
        bool supportsSubgroupSizeControl = false;
        bool supportsPipelineCreationFeedback = false;

        uint32_t maxMeshWorkGroupInvocations = 0;
        uint32_t maxMeshWorkGroupSize[3] = {0, 0, 0};
        uint32_t maxMeshPayloadSize = 0;
        uint32_t maxPreferredMeshWorkGroupInvocations = 0;

        uint32_t subgroupSize = 0;
        VkShaderStageFlags supportedSubgroupStages = 0;

        float timestampPeriod = 0.0f;
        float maxSamplerAnisotropy = 1.0f;
        VkDeviceSize minStorageBufferOffsetAlignment = 256;
        VkDeviceSize minUniformBufferOffsetAlignment = 256;
        uint32_t maxPushConstantsSize = 128;
        VkExtent3D minImageTransferGranularity = {1, 1, 1};
    };

    struct VulkanDeviceConfig
    {
        bool enableValidationLayers = true;
        bool requireMeshShaders = false;
        bool requireDedicatedQueues = true;
        std::vector<const char *> customDeviceExtensions{};
        std::string preferredDeviceName{};
    };

    template<typename T>
    struct VulkanObjectTypeTraits;

#define DEFINE_VK_OBJECT_TYPE_TRAIT(Type, EnumValue) \
        template<> struct VulkanObjectTypeTraits<Type> { \
            static constexpr VkObjectType value = EnumValue; \
        };

    DEFINE_VK_OBJECT_TYPE_TRAIT(VkInstance, VK_OBJECT_TYPE_INSTANCE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkDevice, VK_OBJECT_TYPE_DEVICE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkQueue, VK_OBJECT_TYPE_QUEUE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkFence, VK_OBJECT_TYPE_FENCE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkBuffer, VK_OBJECT_TYPE_BUFFER)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkImage, VK_OBJECT_TYPE_IMAGE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkEvent, VK_OBJECT_TYPE_EVENT)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkPipeline, VK_OBJECT_TYPE_PIPELINE)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkSampler, VK_OBJECT_TYPE_SAMPLER)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkSurfaceKHR, VK_OBJECT_TYPE_SURFACE_KHR)
    DEFINE_VK_OBJECT_TYPE_TRAIT(VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR)

#undef DEFINE_VK_OBJECT_TYPE_TRAIT

    class VulkanDevice;

    class ScopedDebugMarker
    {
    public:
        ScopedDebugMarker(const VulkanDevice &device,
                          VkCommandBuffer cmd,
                          std::string_view name,
                          const std::array<float, 4> &color = {1.0f, 1.0f, 1.0f, 1.0f});
        ~ScopedDebugMarker();

        ScopedDebugMarker(const ScopedDebugMarker &) = delete;
        ScopedDebugMarker &operator=(const ScopedDebugMarker &) = delete;
        ScopedDebugMarker(ScopedDebugMarker &&) noexcept = default;
        ScopedDebugMarker &operator=(ScopedDebugMarker &&) noexcept = default;

    private:
        const VulkanDevice &device;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
    };

    class VulkanDevice
    {
    public:
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface, const VulkanDeviceConfig &config = {});
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice &) = delete;
        VulkanDevice &operator=(const VulkanDevice &) = delete;
        VulkanDevice(VulkanDevice &&other) noexcept;
        VulkanDevice &operator=(VulkanDevice &&other) noexcept;

        ENGINE_NODISCARD VkDevice GetHandle() const noexcept
        {
            return device;
        }

        ENGINE_NODISCARD VkPhysicalDevice GetPhysicalDevice() const noexcept
        {
            return physicalDevice;
        }

        ENGINE_NODISCARD VkInstance GetInstance() const noexcept
        {
            return instance;
        }

        ENGINE_NODISCARD VkSurfaceKHR GetSurface() const noexcept
        {
            return surface;
        }

        ENGINE_NODISCARD VkQueue GetQueue(QueueType type) const noexcept;
        ENGINE_NODISCARD uint32_t GetQueueFamily(QueueType type) const noexcept;
        ENGINE_NODISCARD const QueueFamilyIndices &GetQueueFamilyIndices() const noexcept
        {
            return queueIndices;
        }

        ENGINE_NODISCARD VkQueue GetGraphicsQueue() const noexcept
        {
            return graphicsQueue;
        }

        ENGINE_NODISCARD VkQueue GetComputeQueue() const noexcept
        {
            return computeQueue;
        }

        ENGINE_NODISCARD VkQueue GetTransferQueue() const noexcept
        {
            return transferQueue;
        }

        ENGINE_NODISCARD VkQueue GetPresentQueue() const noexcept
        {
            return presentQueue;
        }

        ENGINE_NODISCARD uint32_t GetGraphicsQueueFamily() const noexcept
        {
            return queueIndices.graphicsFamily;
        }

        ENGINE_NODISCARD uint32_t GetComputeQueueFamily() const noexcept
        {
            return queueIndices.computeFamily;
        }

        ENGINE_NODISCARD uint32_t GetTransferQueueFamily() const noexcept
        {
            return queueIndices.transferFamily;
        }

        ENGINE_NODISCARD uint32_t GetPresentQueueFamily() const noexcept
        {
            return queueIndices.presentFamily;
        }

        ENGINE_NODISCARD std::mutex &GetQueueMutex(QueueType type) const noexcept;

        ENGINE_NODISCARD bool IsMeshShaderSupported() const noexcept
        {
            return capabilities.supportsMeshShaders;
        }

        ENGINE_NODISCARD bool IsExtensionSupported(std::string_view extensionName) const noexcept;
        ENGINE_NODISCARD const DeviceCapabilities &GetCapabilities() const noexcept
        {
            return capabilities;
        }

        ENGINE_NODISCARD const VkPhysicalDeviceProperties2 &GetPhysicalDeviceProperties() const noexcept
        {
            return physicalProperties;
        }

        ENGINE_NODISCARD const VkPhysicalDeviceLimits &GetPhysicalDeviceLimits() const noexcept
        {
            return physicalProperties.properties.limits;
        }

        ENGINE_NODISCARD VkExtent3D GetTransferGranularity() const noexcept
        {
            return capabilities.minImageTransferGranularity;
        }

        ENGINE_NODISCARD const VkPhysicalDeviceSubgroupProperties &GetSubgroupProperties() const noexcept
        {
            return subgroupProperties;
        }

        ENGINE_NODISCARD const VkPhysicalDeviceMeshShaderPropertiesEXT &GetMeshShaderProperties() const noexcept
        {
            return meshShaderProperties;
        }

        ENGINE_NODISCARD const VkPhysicalDeviceMemoryProperties2 &GetMemoryProperties() const noexcept
        {
            return memoryProperties;
        }

        ENGINE_NODISCARD VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates,
                                                      VkImageTiling tiling,
                                                      VkFormatFeatureFlags features) const;
        ENGINE_NODISCARD VkFormat FindDepthFormat() const;
        ENGINE_NODISCARD VkFormatProperties GetFormatProperties(VkFormat format) const;
        ENGINE_NODISCARD bool CheckFormatFeatureSupport(VkFormat format,
                                                        VkFormatFeatureFlags features,
                                                        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL) const;
        ENGINE_NODISCARD SwapChainSupportDetails QuerySwapChainSupport(
            VkSurfaceKHR targetSurface = VK_NULL_HANDLE) const;

        void SetObjectName(uint64_t objectHandle, VkObjectType objectType, std::string_view name) const;

        template<typename T>
        void SetObjectName(T handle, std::string_view name) const
        {
            SetObjectName(reinterpret_cast<uint64_t>(handle), VulkanObjectTypeTraits<T>::value, name);
        }

        void SetObjectTag(uint64_t objectHandle,
                          VkObjectType objectType,
                          uint64_t tag,
                          size_t tagSize,
                          const void *tagData) const;
        void BeginDebugMarker(VkCommandBuffer cmd,
                              std::string_view name,
                              const std::array<float, 4> &color = {1.0f, 1.0f, 1.0f, 1.0f}) const;
        void EndDebugMarker(VkCommandBuffer cmd) const;
        void InsertDebugMarker(VkCommandBuffer cmd,
                               std::string_view name,
                               const std::array<float, 4> &color = {1.0f, 1.0f, 1.0f, 1.0f}) const;

        void BeginQueueLabel(QueueType queueType,
                             std::string_view name,
                             const std::array<float, 4> &color = {1.0f, 1.0f, 1.0f, 1.0f}) const;
        void EndQueueLabel(QueueType queueType) const;

        void ExecuteImmediate(QueueType queueType, const std::function<void(VkCommandBuffer)> &recordFn) const;
        ENGINE_NODISCARD VkCommandBuffer BeginSingleTimeCommands(QueueType queueType) const;
        void EndSingleTimeCommands(VkCommandBuffer cmd, QueueType queueType) const;

        void WaitIdle() const;
        void WaitQueueIdle(QueueType type) const;
        ENGINE_NODISCARD bool IsDeviceLost() const noexcept
        {
            return isDeviceLost;
        }

        struct ExtensionFunctions
        {
            PFN_vkSetDebugUtilsObjectNameEXT pfnSetObjectName = nullptr;
            PFN_vkSetDebugUtilsObjectTagEXT pfnSetObjectTag = nullptr;
            PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginLabel = nullptr;
            PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndLabel = nullptr;
            PFN_vkCmdInsertDebugUtilsLabelEXT pfnCmdInsertLabel = nullptr;
            PFN_vkQueueBeginDebugUtilsLabelEXT pfnQueueBeginLabel = nullptr;
            PFN_vkQueueEndDebugUtilsLabelEXT pfnQueueEndLabel = nullptr;
            PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasksEXT = nullptr;
            PFN_vkCmdDrawMeshTasksIndirectEXT pfnCmdDrawMeshTasksIndirectEXT = nullptr;
            PFN_vkCmdDrawMeshTasksIndirectCountEXT pfnCmdDrawMeshTasksIndirectCountEXT = nullptr;
            PFN_vkGetCalibratedTimestampsEXT pfnGetCalibratedTimestampsEXT = nullptr;
        };

        ENGINE_NODISCARD const ExtensionFunctions &GetExtensionFunctions() const noexcept
        {
            return extFn;
        }

    private:
        void PickPhysicalDevice(const VulkanDeviceConfig &config);
        uint32_t ScorePhysicalDevice(VkPhysicalDevice physDevice, const VulkanDeviceConfig &config) const;
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physDevice) const;
        void CreateLogicalDevice(const VulkanDeviceConfig &config);
        void LoadExtensionFunctions();
        void QueryDeviceCapabilities();
        void CreateImmediateCommandPools();
        void DestroyImmediateCommandPools();

        VkInstance instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;

        QueueFamilyIndices queueIndices;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue computeQueue = VK_NULL_HANDLE;
        VkQueue transferQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;

        mutable std::mutex graphicsQueueMutex;
        mutable std::mutex computeQueueMutex;
        mutable std::mutex transferQueueMutex;
        mutable std::mutex presentQueueMutex;

        DeviceCapabilities capabilities;
        VkPhysicalDeviceProperties2 physicalProperties{};
        VkPhysicalDeviceSubgroupProperties subgroupProperties{};
        VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties{};
        VkPhysicalDeviceMemoryProperties2 memoryProperties{};

        std::vector<std::string> supportedExtensions;
        ExtensionFunctions extFn{};

        struct ImmediateContext
        {
            VkCommandPool pool = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            mutable std::mutex mutex;
        };

        std::array<ImmediateContext, static_cast<size_t>(QueueType::Count)> immediateContexts;

        mutable bool isDeviceLost = false;
    };
} // namespace Engine
