#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanDevice.h"
#include "Core/Assert.h"

namespace Engine {

    VulkanPipeline::VulkanPipeline(VulkanDevice& device,
                                   VkPipeline pipeline,
                                   VkPipelineLayout layout,
                                   VkPipelineBindPoint bindPoint)
        : device(device),
          pipeline(pipeline),
          layout(layout),
          bindPoint(bindPoint)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device.GetHandle(), pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }

        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device.GetHandle(), layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    VulkanPipeline::VulkanPipeline(VulkanPipeline&& other) noexcept
        : device(other.device),
          pipeline(other.pipeline),
          layout(other.layout),
          bindPoint(other.bindPoint)
    {
        other.pipeline = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
    }

    VulkanPipeline& VulkanPipeline::operator=(VulkanPipeline&& other) noexcept
    {
        if (this != &other) {
            ENGINE_ASSERT(&device == &other.device, "Cannot move-assign VulkanPipeline between different VulkanDevices!");

            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device.GetHandle(), pipeline, nullptr);
            }
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device.GetHandle(), layout, nullptr);
            }

            pipeline = other.pipeline;
            layout = other.layout;
            bindPoint = other.bindPoint;

            other.pipeline = VK_NULL_HANDLE;
            other.layout = VK_NULL_HANDLE;
        }
        return *this;
    }

} // namespace Engine