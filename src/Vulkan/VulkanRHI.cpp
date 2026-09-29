#include "Vulkan/VulkanRHI.h"
#include "System/Window/WindowSubsystem.h"
#include "Core/SubsystemRegistry.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include <cstring>
#include <vector>

namespace Engine {
    static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRHIDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_ERROR("VulkanValidation", "{}", callbackData->pMessage);
        } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_WARN("VulkanValidation", "{}", callbackData->pMessage);
        } else {
            LOG_INFO("VulkanValidation", "{}", callbackData->pMessage);
        }
        return VK_FALSE;
    }

    bool VulkanRHI::CreateInstance(bool enableValidation, bool hasWindow)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RenderEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RenderEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        std::vector<const char*> requestedExtensions;
        std::vector<const char*> requestedLayers;

        if (hasWindow) {
#if defined(_WIN32)
            requestedExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
            requestedExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
        }

        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        bool validationSupported = false;
        if (enableValidation) {
            for (const auto& layer : availableLayers) {
                if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                    validationSupported = true;
                    break;
                }
            }
            if (validationSupported) {
                requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
                requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
        }

        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());

        std::vector<const char*> enabledExtensions;
        for (const char* req : requestedExtensions) {
            for (const auto& avail : availableExts) {
                if (std::strcmp(req, avail.extensionName) == 0) {
                    enabledExtensions.push_back(req);
                    break;
                }
            }
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size());
        createInfo.ppEnabledLayerNames = requestedLayers.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (validationSupported) {
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = VulkanRHIDebugCallback;
            createInfo.pNext = &debugCreateInfo;
        }

        VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
        if (res != VK_SUCCESS) {
            LOG_FATAL("VulkanRHI", "Failed to create Vulkan instance: {}", static_cast<int>(res));
            return false;
        }

        if (validationSupported) {
            SetupDebugMessenger();
        }

        return true;
    }

    bool VulkanRHI::SetupDebugMessenger()
    {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!func) {
            return false;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanRHIDebugCallback;

        return func(instance, &createInfo, nullptr, &debugMessenger) == VK_SUCCESS;
    }

    bool VulkanRHI::Initialize(SubsystemRegistry& registry)
    {
        WindowSubsystem* windowSubsystem = registry.TryGet<WindowSubsystem>();
        bool hasWindow = (windowSubsystem != nullptr);

        if (!CreateInstance(true, hasWindow)) {
            return false;
        }

        if (hasWindow) {
            IWindow& window = windowSubsystem->GetWindow();
            if (!window.CreateVulkanSurface(instance, &surface)) {
                LOG_FATAL("VulkanRHI", "Failed to create Vulkan surface from window");
                return false;
            }
        }

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = true;

        device = std::make_unique<VulkanDevice>(instance, surface, config);
        memory = std::make_unique<VulkanMemory>(*device);
        bindlessHeap = std::make_unique<VulkanBindlessHeap>(*device);
        pipelineCache = std::make_unique<VulkanPipelineCache>(*device, bindlessHeap->GetDescriptorSetLayout());
        queueManager = std::make_unique<VulkanQueueManager>(*device);

        if (hasWindow && surface != VK_NULL_HANDLE) {
            IWindow& window = windowSubsystem->GetWindow();
            swapchain = std::make_unique<VulkanSwapchain>(*device, window, surface, window.IsVSync());
        }

        LOG_INFO("VulkanRHI", "VulkanRHI initialized successfully");
        return true;
    }

    void VulkanRHI::Update(float deltaTime)
    {
        ISubsystem::Update(deltaTime);
    }

    void VulkanRHI::Shutdown()
    {
        LOG_INFO("VulkanRHI", "Shutting down VulkanRHI...");

        if (device) {
            device->WaitIdle();
        }

        swapchain.reset();
        pipelineCache.reset();
        bindlessHeap.reset();
        queueManager.reset();
        memory.reset();
        device.reset();

        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }

        if (debugMessenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (func) {
                func(instance, debugMessenger, nullptr);
            }
            debugMessenger = VK_NULL_HANDLE;
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }

        LOG_INFO("VulkanRHI", "VulkanRHI shutdown complete");
    }
}
