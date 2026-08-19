#include "Common/VulkanTestContext.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace Engine::Test {

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::cerr << "[Vulkan Validation] " << (callbackData->pMessage ? callbackData->pMessage : "") << "\n";
        }
        return VK_FALSE;
    }

    VulkanTestContext& VulkanTestContext::Get() {
        static VulkanTestContext instance;
        return instance;
    }

    VulkanTestContext::VulkanTestContext() {
    }

    VulkanTestContext::~VulkanTestContext() {
        Shutdown();
    }

    bool VulkanTestContext::Initialize() {
        if (initialized) {
            return true;
        }

        if (!createInstance()) {
            return false;
        }

        if (!pickPhysicalDevice()) {
            Shutdown();
            return false;
        }

        if (!createLogicalDevice()) {
            Shutdown();
            return false;
        }

        if (!createAllocator()) {
            Shutdown();
            return false;
        }

        initialized = true;
        return true;
    }

    void VulkanTestContext::Shutdown() {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }

        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }

        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        if (debugMessenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) {
                func(instance, debugMessenger, nullptr);
            }
            debugMessenger = VK_NULL_HANDLE;
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }

        initialized = false;
    }

    void VulkanTestContext::WaitIdle() const {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    bool VulkanTestContext::SupportsExtension(const char* extensionName) const {
        for (const auto& ext : availableExtensions) {
            if (std::strcmp(ext.extensionName, extensionName) == 0) {
                return true;
            }
        }
        return false;
    }

    bool VulkanTestContext::createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RenderEngine Headless Tests";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RenderEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        std::vector<const char*> instanceExtensions;
        std::vector<const char*> instanceLayers;

        // Check for validation layers
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        bool validationFound = false;
        for (const auto& layer : availableLayers) {
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validationFound = true;
                break;
            }
        }

        // Check for debug utils extension
        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> instExts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, instExts.data());

        bool debugUtilsFound = false;
        for (const auto& ext : instExts) {
            if (std::strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                debugUtilsFound = true;
                break;
            }
        }

        if (validationFound) {
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
        }
        if (debugUtilsFound) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
        createInfo.ppEnabledLayerNames = instanceLayers.empty() ? nullptr : instanceLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        createInfo.ppEnabledExtensionNames = instanceExtensions.empty() ? nullptr : instanceExtensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (debugUtilsFound && validationFound) {
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;
            createInfo.pNext = &debugCreateInfo;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            return false;
        }

        if (debugUtilsFound && validationFound) {
            auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
            if (createFunc) {
                createFunc(instance, &debugCreateInfo, nullptr, &debugMessenger);
            }
        }

        return true;
    }

    bool VulkanTestContext::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Prioritize discrete GPU
        for (const auto& dev : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physicalDevice = dev;
                properties = props;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            physicalDevice = devices[0];
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        }

        // Query available device extensions
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        availableExtensions.resize(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExtensions.data());

        bdaSupported = SupportsExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        meshShaderSupported = SupportsExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        sync2Supported = SupportsExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        dynamicRenderingSupported = SupportsExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily = i;
            }
            if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                computeFamily = i;
            }
        }

        // Fallback compute family to graphics family if no dedicated compute family exists
        if (computeFamily == ~0u) {
            computeFamily = graphicsFamily;
        }

        return (graphicsFamily != ~0u);
    }

    bool VulkanTestContext::createLogicalDevice() {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::vector<uint32_t> uniqueFamilies = { graphicsFamily };
        if (computeFamily != graphicsFamily && computeFamily != ~0u) {
            uniqueFamilies.push_back(computeFamily);
        }

        float queuePriority = 1.0f;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueInfo);
        }

        std::vector<const char*> enabledExtensions;
        if (bdaSupported) {
            enabledExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        }
        if (meshShaderSupported) {
            enabledExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }
        if (sync2Supported) {
            enabledExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        }
        if (dynamicRenderingSupported) {
            enabledExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }

        // Build pNext feature chain for Vulkan 1.3
        meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        meshShaderFeatures.meshShader = meshShaderSupported ? VK_TRUE : VK_FALSE;
        meshShaderFeatures.taskShader = meshShaderSupported ? VK_TRUE : VK_FALSE;

        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.synchronization2 = VK_TRUE;
        features13.dynamicRendering = VK_TRUE;
        features13.maintenance4 = VK_TRUE;
        features13.pNext = meshShaderSupported ? &meshShaderFeatures : nullptr;

        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.timelineSemaphore = VK_TRUE;
        features12.scalarBlockLayout = VK_TRUE;
        features12.pNext = &features13;

        features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features11.multiview = VK_TRUE;
        features11.pNext = &features12;

        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.features.samplerAnisotropy = VK_TRUE;
        features2.features.depthClamp = VK_TRUE;
        features2.features.fillModeNonSolid = VK_TRUE;
        features2.features.geometryShader = VK_TRUE;
        features2.pNext = &features11;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();
        createInfo.pNext = &features2;

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            return false;
        }

        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);

        return true;
    }

    bool VulkanTestContext::createAllocator() {
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo{};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorCreateInfo.physicalDevice = physicalDevice;
        allocatorCreateInfo.device = device;
        allocatorCreateInfo.instance = instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
        if (bdaSupported) {
            allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        return (vmaCreateAllocator(&allocatorCreateInfo, &allocator) == VK_SUCCESS);
    }

} // namespace Engine::Test
