#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "Core/CoreDefines.h"
#include "Vulkan/VulkanTexture.h"

namespace Engine {
    class VulkanDevice;
    class IWindow;

    class VulkanSwapchain {
    public:
        VulkanSwapchain(VulkanDevice& device, IWindow& window, VkSurfaceKHR surface, bool vsync = true);
        ~VulkanSwapchain();

        ENGINE_NON_COPYABLE(VulkanSwapchain);

        void Recreate(uint32_t newWidth, uint32_t newHeight, bool vsync);

        VkResult AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* outImageIndex);
        VkResult Present(VkQueue presentQueue, uint32_t imageIndex, VkSemaphore renderFinishedSemaphore);

        ENGINE_NODISCARD VkFormat GetFormat() const noexcept { return surfaceFormat.format; }
        ENGINE_NODISCARD VkExtent2D GetExtent() const noexcept { return extent; }
        ENGINE_NODISCARD uint32_t GetImageCount() const noexcept { return static_cast<uint32_t>(images.size()); }
        ENGINE_NODISCARD VulkanTexture& GetTexture(uint32_t index) noexcept { return *images[index]; }
        ENGINE_NODISCARD const VulkanTexture& GetTexture(uint32_t index) const noexcept { return *images[index]; }
        ENGINE_NODISCARD VkSwapchainKHR GetHandle() const noexcept { return swapchain; }

    private:
        void Create(uint32_t width, uint32_t height, bool vsync);
        void Destroy();

        VulkanDevice& device;
        IWindow& window;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSurfaceFormatKHR surfaceFormat{};
        VkPresentModeKHR presentMode{};
        VkExtent2D extent{ 0, 0 };

        std::vector<std::unique_ptr<VulkanTexture>> images;
    };
}
