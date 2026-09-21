#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "Core/CoreDefines.h"

namespace Engine {
    class VulkanDevice;

    enum class CullMode { 
        None  = VK_CULL_MODE_NONE, 
        Back  = VK_CULL_MODE_BACK_BIT, 
        Front = VK_CULL_MODE_FRONT_BIT 
    };

    enum class CompareOp { 
        Less        = VK_COMPARE_OP_LESS, 
        LessOrEqual = VK_COMPARE_OP_LESS_OR_EQUAL, 
        Greater     = VK_COMPARE_OP_GREATER, 
        Always      = VK_COMPARE_OP_ALWAYS 
    };

    enum class BlendMode { 
        Opaque, 
        AlphaBlend, 
        Additive 
    };

    struct GraphicsPipelineDesc {
        std::string debugName;
        std::string vertexShaderPath;
        std::string fragmentShaderPath;
        std::string taskShaderPath;
        std::string meshShaderPath;

        CullMode cullMode = CullMode::Back;
        bool depthTest = true;
        bool depthWrite = true;
        CompareOp depthCompareOp = CompareOp::LessOrEqual;
        BlendMode blendMode = BlendMode::Opaque;

        std::vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        uint32_t pushConstantSize = 128;
    };

    struct ComputePipelineDesc {
        std::string debugName;
        std::string computeShaderPath;
        uint32_t pushConstantSize = 128;
    };

    class VulkanPipeline {
    public:
        VulkanPipeline(VulkanDevice& device, 
                       VkPipeline pipeline, 
                       VkPipelineLayout layout, 
                       VkPipelineBindPoint bindPoint);
        ~VulkanPipeline();

        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;
        VulkanPipeline(VulkanPipeline&& other) noexcept;
        VulkanPipeline& operator=(VulkanPipeline&& other) noexcept;

        ENGINE_NODISCARD VkPipeline GetPipeline() const noexcept { return pipeline; }
        ENGINE_NODISCARD VkPipelineLayout GetLayout() const noexcept { return layout; }
        ENGINE_NODISCARD VkPipelineBindPoint GetBindPoint() const noexcept { return bindPoint; }

    private:
        VulkanDevice& device;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint;
    };

} // namespace Engine
