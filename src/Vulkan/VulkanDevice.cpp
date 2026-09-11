#include "VulkanDevice.h"
#include "Core/Log.h"
#include <algorithm>
#include <cstring>

namespace Engine {
    ScopedDebugMarker::ScopedDebugMarker(const VulkanDevice& inDevice,
                                         VkCommandBuffer inCmd,
                                         std::string_view name,
                                         const std::array<float, 4>& color)
        : device(inDevice), cmd(inCmd)
    {
        device.BeginDebugMarker(cmd, name, color);
    }

    ScopedDebugMarker::~ScopedDebugMarker() {
        if (cmd != VK_NULL_HANDLE) {
            device.EndDebugMarker(cmd);
        }
    }

    VulkanDevice::VulkanDevice(VkInstance inInstance, VkSurfaceKHR inSurface, const VulkanDeviceConfig& config)
        : instance(inInstance), surface(inSurface)
    {
        LOG_INFO("Vulkan", "Initializing VulkanDevice subsystem...");
        PickPhysicalDevice(config);
        CreateLogicalDevice(config);
        LoadExtensionFunctions();
        QueryDeviceCapabilities();
        CreateImmediateCommandPools();
        LOG_INFO("Vulkan", "VulkanDevice initialized successfully.");
    }

    VulkanDevice::~VulkanDevice() {
        if (device != VK_NULL_HANDLE) {
            WaitIdle();
            DestroyImmediateCommandPools();
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
            LOG_INFO("Vulkan", "VulkanDevice destroyed.");
        }
    }

    VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
        : instance(other.instance),
          surface(other.surface),
          physicalDevice(other.physicalDevice),
          device(other.device),
          queueIndices(other.queueIndices),
          graphicsQueue(other.graphicsQueue),
          computeQueue(other.computeQueue),
          transferQueue(other.transferQueue),
          presentQueue(other.presentQueue),
          capabilities(other.capabilities),
          physicalProperties(other.physicalProperties),
          subgroupProperties(other.subgroupProperties),
          meshShaderProperties(other.meshShaderProperties),
          memoryProperties(other.memoryProperties),
          supportedExtensions(std::move(other.supportedExtensions)),
          extFn(other.extFn),
          isDeviceLost(other.isDeviceLost)
    {
        for (size_t i = 0; i < immediateContexts.size(); ++i) {
            immediateContexts[i].pool = other.immediateContexts[i].pool;
            immediateContexts[i].fence = other.immediateContexts[i].fence;
            other.immediateContexts[i].pool = VK_NULL_HANDLE;
            other.immediateContexts[i].fence = VK_NULL_HANDLE;
        }

        other.device = VK_NULL_HANDLE;
        other.physicalDevice = VK_NULL_HANDLE;
        other.graphicsQueue = VK_NULL_HANDLE;
        other.computeQueue = VK_NULL_HANDLE;
        other.transferQueue = VK_NULL_HANDLE;
        other.presentQueue = VK_NULL_HANDLE;
    }

    VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept {
        if (this != &other) {
            if (device != VK_NULL_HANDLE) {
                WaitIdle();
                DestroyImmediateCommandPools();
                vkDestroyDevice(device, nullptr);
            }

            instance = other.instance;
            surface = other.surface;
            physicalDevice = other.physicalDevice;
            device = other.device;
            queueIndices = other.queueIndices;
            graphicsQueue = other.graphicsQueue;
            computeQueue = other.computeQueue;
            transferQueue = other.transferQueue;
            presentQueue = other.presentQueue;
            capabilities = other.capabilities;
            physicalProperties = other.physicalProperties;
            subgroupProperties = other.subgroupProperties;
            meshShaderProperties = other.meshShaderProperties;
            memoryProperties = other.memoryProperties;
            supportedExtensions = std::move(other.supportedExtensions);
            extFn = other.extFn;
            isDeviceLost = other.isDeviceLost;

            for (size_t i = 0; i < immediateContexts.size(); ++i) {
                immediateContexts[i].pool = other.immediateContexts[i].pool;
                immediateContexts[i].fence = other.immediateContexts[i].fence;
                other.immediateContexts[i].pool = VK_NULL_HANDLE;
                other.immediateContexts[i].fence = VK_NULL_HANDLE;
            }

            other.device = VK_NULL_HANDLE;
            other.physicalDevice = VK_NULL_HANDLE;
            other.graphicsQueue = VK_NULL_HANDLE;
            other.computeQueue = VK_NULL_HANDLE;
            other.transferQueue = VK_NULL_HANDLE;
            other.presentQueue = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkQueue VulkanDevice::GetQueue(QueueType type) const noexcept {
        switch (type) {
            case QueueType::Graphics: return graphicsQueue;
            case QueueType::Compute:  return computeQueue;
            case QueueType::Transfer: return transferQueue;
            case QueueType::Present:  return presentQueue;
            default:                  return VK_NULL_HANDLE;
        }
    }

    uint32_t VulkanDevice::GetQueueFamily(QueueType type) const noexcept {
        switch (type) {
            case QueueType::Graphics: return queueIndices.graphicsFamily;
            case QueueType::Compute:  return queueIndices.computeFamily;
            case QueueType::Transfer: return queueIndices.transferFamily;
            case QueueType::Present:  return queueIndices.presentFamily;
            default:                  return VK_QUEUE_FAMILY_IGNORED;
        }
    }

    std::mutex& VulkanDevice::GetQueueMutex(QueueType type) const noexcept {
        switch (type) {
            case QueueType::Graphics: return graphicsQueueMutex;
            case QueueType::Compute:  return computeQueueMutex;
            case QueueType::Transfer: return transferQueueMutex;
            case QueueType::Present:  return presentQueueMutex;
            default:                  return graphicsQueueMutex;
        }
    }

    bool VulkanDevice::IsExtensionSupported(std::string_view extensionName) const noexcept {
        return std::find(supportedExtensions.begin(), supportedExtensions.end(), extensionName) != supportedExtensions.end();
    }

    VkFormat VulkanDevice::FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                               VkImageTiling tiling,
                                               VkFormatFeatureFlags features) const
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        ENGINE_ASSERT(false, "Failed to find supported format!");
        return VK_FORMAT_UNDEFINED;
    }

    VkFormat VulkanDevice::FindDepthFormat() const {
        return FindSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormatProperties VulkanDevice::GetFormatProperties(VkFormat format) const {
        VkFormatProperties props{};
        if (physicalDevice != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        }
        return props;
    }

    bool VulkanDevice::CheckFormatFeatureSupport(VkFormat format,
                                                 VkFormatFeatureFlags features,
                                                 VkImageTiling tiling) const
    {
        VkFormatProperties props = GetFormatProperties(format);
        if (tiling == VK_IMAGE_TILING_LINEAR) {
            return (props.linearTilingFeatures & features) == features;
        }
        return (props.optimalTilingFeatures & features) == features;
    }

    SwapChainSupportDetails VulkanDevice::QuerySwapChainSupport(VkSurfaceKHR targetSurface) const {
        SwapChainSupportDetails details{};
        VkSurfaceKHR querySurf = (targetSurface != VK_NULL_HANDLE) ? targetSurface : surface;
        if (physicalDevice == VK_NULL_HANDLE || querySurf == VK_NULL_HANDLE) {
            return details;
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, querySurf, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, querySurf, &formatCount, nullptr);
        if (formatCount > 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, querySurf, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, querySurf, &presentModeCount, nullptr);
        if (presentModeCount > 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, querySurf, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    void VulkanDevice::SetObjectName(uint64_t objectHandle, VkObjectType objectType, std::string_view name) const {
        if (extFn.pfnSetObjectName && device != VK_NULL_HANDLE && objectHandle != 0) {
            std::string nullTerminatedName(name);
            VkDebugUtilsObjectNameInfoEXT info{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = nullptr,
                .objectType = objectType,
                .objectHandle = objectHandle,
                .pObjectName = nullTerminatedName.c_str()
            };
            extFn.pfnSetObjectName(device, &info);
        }
    }

    void VulkanDevice::SetObjectTag(uint64_t objectHandle, VkObjectType objectType, uint64_t tag, size_t tagSize, const void* tagData) const {
        if (extFn.pfnSetObjectTag && device != VK_NULL_HANDLE && objectHandle != 0) {
            VkDebugUtilsObjectTagInfoEXT info{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT,
                .pNext = nullptr,
                .objectType = objectType,
                .objectHandle = objectHandle,
                .tagName = tag,
                .tagSize = tagSize,
                .pTag = tagData
            };
            extFn.pfnSetObjectTag(device, &info);
        }
    }

    void VulkanDevice::BeginDebugMarker(VkCommandBuffer cmd, std::string_view name, const std::array<float, 4>& color) const {
        if (extFn.pfnCmdBeginLabel && cmd != VK_NULL_HANDLE) {
            std::string nullTerminatedName(name);
            VkDebugUtilsLabelEXT label{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = nullTerminatedName.c_str(),
                .color = { color[0], color[1], color[2], color[3] }
            };
            extFn.pfnCmdBeginLabel(cmd, &label);
        }
    }

    void VulkanDevice::EndDebugMarker(VkCommandBuffer cmd) const {
        if (extFn.pfnCmdEndLabel && cmd != VK_NULL_HANDLE) {
            extFn.pfnCmdEndLabel(cmd);
        }
    }

    void VulkanDevice::InsertDebugMarker(VkCommandBuffer cmd, std::string_view name, const std::array<float, 4>& color) const {
        if (extFn.pfnCmdInsertLabel && cmd != VK_NULL_HANDLE) {
            std::string nullTerminatedName(name);
            VkDebugUtilsLabelEXT label{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = nullTerminatedName.c_str(),
                .color = { color[0], color[1], color[2], color[3] }
            };
            extFn.pfnCmdInsertLabel(cmd, &label);
        }
    }

    void VulkanDevice::BeginQueueLabel(QueueType queueType, std::string_view name, const std::array<float, 4>& color) const {
        VkQueue queue = GetQueue(queueType);
        if (extFn.pfnQueueBeginLabel && queue != VK_NULL_HANDLE) {
            std::string nullTerminatedName(name);
            VkDebugUtilsLabelEXT label{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = nullTerminatedName.c_str(),
                .color = { color[0], color[1], color[2], color[3] }
            };
            extFn.pfnQueueBeginLabel(queue, &label);
        }
    }

    void VulkanDevice::EndQueueLabel(QueueType queueType) const {
        VkQueue queue = GetQueue(queueType);
        if (extFn.pfnQueueEndLabel && queue != VK_NULL_HANDLE) {
            extFn.pfnQueueEndLabel(queue);
        }
    }

    void VulkanDevice::ExecuteImmediate(QueueType queueType, const std::function<void(VkCommandBuffer)>& recordFn) const {
        VkCommandBuffer cmd = BeginSingleTimeCommands(queueType);
        recordFn(cmd);
        EndSingleTimeCommands(cmd, queueType);
    }

    VkCommandBuffer VulkanDevice::BeginSingleTimeCommands(QueueType queueType) const {
        size_t idx = static_cast<size_t>(queueType);
        auto& ctx = immediateContexts[idx];
        ctx.mutex.lock();

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = ctx.pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void VulkanDevice::EndSingleTimeCommands(VkCommandBuffer cmd, QueueType queueType) const {
        size_t idx = static_cast<size_t>(queueType);
        auto& ctx = immediateContexts[idx];

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
        };

        VkQueue queue = GetQueue(queueType);
        std::mutex& queueMutex = GetQueueMutex(queueType);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            vkResetFences(device, 1, &ctx.fence);
            vkQueueSubmit(queue, 1, &submitInfo, ctx.fence);
        }

        vkWaitForFences(device, 1, &ctx.fence, VK_TRUE, UINT64_MAX);
        vkFreeCommandBuffers(device, ctx.pool, 1, &cmd);

        ctx.mutex.unlock();
    }

    void VulkanDevice::WaitIdle() const {
        if (device != VK_NULL_HANDLE) {
            VkResult res = vkDeviceWaitIdle(device);
            if (res == VK_ERROR_DEVICE_LOST) {
                isDeviceLost = true;
                LOG_FATAL("Vulkan", "VK_ERROR_DEVICE_LOST encountered during vkDeviceWaitIdle!");
            }
        }
    }

    void VulkanDevice::WaitQueueIdle(QueueType type) const {
        VkQueue q = GetQueue(type);
        if (q != VK_NULL_HANDLE) {
            std::lock_guard<std::mutex> lock(GetQueueMutex(type));
            VkResult res = vkQueueWaitIdle(q);
            if (res == VK_ERROR_DEVICE_LOST) {
                isDeviceLost = true;
                LOG_FATAL("Vulkan", "VK_ERROR_DEVICE_LOST encountered during vkQueueWaitIdle!");
            }
        }
    }

    void VulkanDevice::PickPhysicalDevice(const VulkanDeviceConfig& config) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        ENGINE_ASSERT(deviceCount > 0, "No Vulkan-capable physical devices discovered!");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        uint32_t bestScore = 0;
        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

        for (VkPhysicalDevice pd : devices) {
            uint32_t score = ScorePhysicalDevice(pd, config);
            if (score > bestScore) {
                bestScore = score;
                bestDevice = pd;
            }
        }

        ENGINE_ASSERT(bestDevice != VK_NULL_HANDLE, "Failed to find a suitable GPU meeting hardware requirements!");
        physicalDevice = bestDevice;
        queueIndices = FindQueueFamilies(physicalDevice);

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExtensions.data());
        supportedExtensions.clear();
        supportedExtensions.reserve(extCount);
        for (const auto& ext : availableExtensions) {
            supportedExtensions.emplace_back(ext.extensionName);
        }
    }

    uint32_t VulkanDevice::ScorePhysicalDevice(VkPhysicalDevice physDevice, const VulkanDeviceConfig& config) const {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(physDevice, &props);
        vkGetPhysicalDeviceFeatures(physDevice, &features);

        if (!features.samplerAnisotropy) {
            return 0;
        }

        auto indices = FindQueueFamilies(physDevice);
        if (!indices.IsComplete()) {
            return 0;
        }

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, availableExtensions.data());

        bool hasSwapchain = false;
        bool hasMeshShader = false;
        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
            }
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                hasMeshShader = true;
            }
        }

        if (surface != VK_NULL_HANDLE && !hasSwapchain) {
            return 0;
        }

        if (config.requireMeshShaders && !hasMeshShader) {
            return 0;
        }

        for (const char* customExt : config.customDeviceExtensions) {
            bool found = false;
            for (const auto& ext : availableExtensions) {
                if (strcmp(ext.extensionName, customExt) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return 0;
            }
        }

        uint32_t score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 1000;
        }

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                score += static_cast<uint32_t>(memProps.memoryHeaps[i].size / (64_MB));
            }
        }

        if (hasMeshShader) {
            score += 5000;
        }

        if (config.requireDedicatedQueues && indices.IsComputeDedicated() && indices.IsTransferDedicated()) {
            score += 2000;
        }

        if (!config.preferredDeviceName.empty() &&
            std::string_view(props.deviceName).find(config.preferredDeviceName) != std::string_view::npos) {
            score += 50000;
        }

        return score;
    }

    QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice physDevice) const {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueCount == 0) {
                continue;
            }

            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentSupport = false;
                if (surface != VK_NULL_HANDLE) {
                    vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface, &presentSupport);
                } else {
                    presentSupport = true; // offscreen or headless
                }

                if (presentSupport) {
                    indices.graphicsFamily = i;
                    indices.presentFamily = i;
                    break;
                }
            }
        }

        if (indices.graphicsFamily == VK_QUEUE_FAMILY_IGNORED || indices.presentFamily == VK_QUEUE_FAMILY_IGNORED) {
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueCount == 0) {
                    continue;
                }

                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && indices.graphicsFamily == VK_QUEUE_FAMILY_IGNORED) {
                    indices.graphicsFamily = i;
                }

                if (surface != VK_NULL_HANDLE) {
                    VkBool32 presentSupport = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface, &presentSupport);
                    if (presentSupport && indices.presentFamily == VK_QUEUE_FAMILY_IGNORED) {
                        indices.presentFamily = i;
                    }
                }
            }

            if (surface == VK_NULL_HANDLE) {
                indices.presentFamily = indices.graphicsFamily;
            }
        }

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueCount > 0 &&
                (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
                break;
            }
        }

        if (indices.computeFamily == VK_QUEUE_FAMILY_IGNORED) {
            indices.computeFamily = indices.graphicsFamily;
        }

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueCount > 0 &&
                (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.transferFamily = i;
                break;
            }
        }

        if (indices.transferFamily == VK_QUEUE_FAMILY_IGNORED) {
            for (uint32_t i = 0; i < queueFamilyCount; ++i) {
                if (queueFamilies[i].queueCount > 0 &&
                    (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    indices.transferFamily = i;
                    break;
                }
            }
        }

        if (indices.transferFamily == VK_QUEUE_FAMILY_IGNORED) {
            indices.transferFamily = (indices.computeFamily != VK_QUEUE_FAMILY_IGNORED)
                                         ? indices.computeFamily
                                         : indices.graphicsFamily;
        }

        return indices;
    }

    void VulkanDevice::CreateLogicalDevice(const VulkanDeviceConfig& config) {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<uint32_t> uniqueQueueFamilies = {
            queueIndices.graphicsFamily,
            queueIndices.computeFamily,
            queueIndices.transferFamily,
            queueIndices.presentFamily
        };
        std::sort(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());
        uniqueQueueFamilies.erase(std::unique(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end()), uniqueQueueFamilies.end());

        float queuePriority = 1.0f;
        for (uint32_t family : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority
            };
            queueCreateInfos.push_back(queueInfo);
        }

        std::vector<const char*> enabledExtensions;
        if (surface != VK_NULL_HANDLE && IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        bool enableMeshShader = IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        if (enableMeshShader) {
            enabledExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }

        if (IsExtensionSupported(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
            enabledExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        }

        if (IsExtensionSupported(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
            enabledExtensions.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
        }

        for (const char* ext : config.customDeviceExtensions) {
            if (IsExtensionSupported(ext)) {
                enabledExtensions.push_back(ext);
            }
        }

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.synchronization2 = VK_TRUE;
        features13.dynamicRendering = VK_TRUE;
        features13.shaderDemoteToHelperInvocation = VK_TRUE;
        features13.maintenance4 = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.pNext = &features13;
        features12.descriptorIndexing = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.scalarBlockLayout = VK_TRUE;
        features12.timelineSemaphore = VK_TRUE;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.drawIndirectCount = VK_TRUE;
        features12.shaderOutputLayer = VK_TRUE;
        features12.storageBuffer8BitAccess = VK_TRUE;
        features12.hostQueryReset = VK_TRUE;
        features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;

        VkPhysicalDeviceVulkan11Features features11{};
        features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features11.pNext = &features12;
        features11.multiview = VK_TRUE;
        features11.shaderDrawParameters = VK_TRUE;

        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
        meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        meshShaderFeatures.pNext = &features11;
        meshShaderFeatures.taskShader = VK_TRUE;
        meshShaderFeatures.meshShader = VK_TRUE;
        meshShaderFeatures.multiviewMeshShader = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = enableMeshShader ? static_cast<void*>(&meshShaderFeatures) : static_cast<void*>(&features11);
        deviceFeatures2.features.geometryShader = VK_TRUE;
        deviceFeatures2.features.multiDrawIndirect = VK_TRUE;
        deviceFeatures2.features.drawIndirectFirstInstance = VK_TRUE;
        deviceFeatures2.features.depthClamp = VK_TRUE;
        deviceFeatures2.features.fillModeNonSolid = VK_TRUE;
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &deviceFeatures2,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
            .ppEnabledExtensionNames = enabledExtensions.data()
        };

        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
        ENGINE_ASSERT(res == VK_SUCCESS, "Failed to create Vulkan logical device!");

        vkGetDeviceQueue(device, queueIndices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueIndices.computeFamily, 0, &computeQueue);
        vkGetDeviceQueue(device, queueIndices.transferFamily, 0, &transferQueue);
        vkGetDeviceQueue(device, queueIndices.presentFamily, 0, &presentQueue);
    }

    void VulkanDevice::LoadExtensionFunctions() {
        extFn.pfnSetObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        extFn.pfnSetObjectTag = reinterpret_cast<PFN_vkSetDebugUtilsObjectTagEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectTagEXT"));
        extFn.pfnCmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
        extFn.pfnCmdEndLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
        extFn.pfnCmdInsertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
        extFn.pfnQueueBeginLabel = reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkQueueBeginDebugUtilsLabelEXT"));
        extFn.pfnQueueEndLabel = reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkQueueEndDebugUtilsLabelEXT"));

        extFn.pfnCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
            vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
        extFn.pfnCmdDrawMeshTasksIndirectEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(
            vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectEXT"));
        extFn.pfnCmdDrawMeshTasksIndirectCountEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(
            vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectCountEXT"));
        extFn.pfnGetCalibratedTimestampsEXT = reinterpret_cast<PFN_vkGetCalibratedTimestampsEXT>(
            vkGetDeviceProcAddr(device, "vkGetCalibratedTimestampsEXT"));
    }

    void VulkanDevice::QueryDeviceCapabilities() {
        bool meshSupported = IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME);

        physicalProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        subgroupProperties.pNext = nullptr;
        physicalProperties.pNext = &subgroupProperties;

        if (meshSupported) {
            meshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
            meshShaderProperties.pNext = physicalProperties.pNext;
            physicalProperties.pNext = &meshShaderProperties;
        }

        vkGetPhysicalDeviceProperties2(physicalDevice, &physicalProperties);

        memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memoryProperties);

        capabilities.supportsDynamicRendering = true;
        capabilities.supportsSynchronization2 = true;
        capabilities.supportsMaintenance4 = true;
        capabilities.supportsBufferDeviceAddress = true;
        capabilities.supportsDescriptorIndexing = true;
        capabilities.supportsTimelineSemaphores = true;

        capabilities.supportsMeshShaders = meshSupported;
        capabilities.supportsTaskShaders = meshSupported;
        capabilities.supportsMemoryBudget = IsExtensionSupported(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        capabilities.supportsCalibratedTimestamps = IsExtensionSupported(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

        if (meshSupported) {
            capabilities.maxMeshWorkGroupInvocations = meshShaderProperties.maxMeshWorkGroupInvocations;
            capabilities.maxMeshWorkGroupSize[0] = meshShaderProperties.maxMeshWorkGroupSize[0];
            capabilities.maxMeshWorkGroupSize[1] = meshShaderProperties.maxMeshWorkGroupSize[1];
            capabilities.maxMeshWorkGroupSize[2] = meshShaderProperties.maxMeshWorkGroupSize[2];
            capabilities.maxMeshPayloadSize = meshShaderProperties.maxTaskPayloadSize;
            capabilities.maxPreferredMeshWorkGroupInvocations = meshShaderProperties.maxPreferredMeshWorkGroupInvocations;
        }

        capabilities.subgroupSize = subgroupProperties.subgroupSize;
        capabilities.supportedSubgroupStages = subgroupProperties.supportedStages;

        capabilities.timestampPeriod = physicalProperties.properties.limits.timestampPeriod;
        capabilities.maxSamplerAnisotropy = physicalProperties.properties.limits.maxSamplerAnisotropy;
        capabilities.minStorageBufferOffsetAlignment = physicalProperties.properties.limits.minStorageBufferOffsetAlignment;
        capabilities.minUniformBufferOffsetAlignment = physicalProperties.properties.limits.minUniformBufferOffsetAlignment;
        capabilities.maxPushConstantsSize = physicalProperties.properties.limits.maxPushConstantsSize;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount > 0 && queueIndices.transferFamily < queueFamilyCount) {
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
            capabilities.minImageTransferGranularity = queueFamilies[queueIndices.transferFamily].minImageTransferGranularity;
        }
    }

    void VulkanDevice::CreateImmediateCommandPools() {
        for (size_t i = 0; i < static_cast<size_t>(QueueType::Count); ++i) {
            QueueType qType = static_cast<QueueType>(i);
            uint32_t family = GetQueueFamily(qType);

            VkCommandPoolCreateInfo poolInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = family
            };
            vkCreateCommandPool(device, &poolInfo, nullptr, &immediateContexts[i].pool);

            VkFenceCreateInfo fenceInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = 0
            };
            vkCreateFence(device, &fenceInfo, nullptr, &immediateContexts[i].fence);
        }
    }

    void VulkanDevice::DestroyImmediateCommandPools() {
        for (auto& ctx : immediateContexts) {
            if (ctx.fence != VK_NULL_HANDLE) {
                vkDestroyFence(device, ctx.fence, nullptr);
                ctx.fence = VK_NULL_HANDLE;
            }
            if (ctx.pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, ctx.pool, nullptr);
                ctx.pool = VK_NULL_HANDLE;
            }
        }
    }

} // namespace Engine