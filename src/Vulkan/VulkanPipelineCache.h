#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/CoreDefines.h"
#include "Vulkan/VulkanPipeline.h"

namespace Engine {

    class VulkanDevice;

    class VulkanPipelineCache {
    public:
        explicit VulkanPipelineCache(VulkanDevice& device, VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);
        ~VulkanPipelineCache();

        VulkanPipelineCache(const VulkanPipelineCache&) = delete;
        VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;

        std::shared_ptr<VulkanPipeline> GetOrCreateGraphicsPipeline(const GraphicsPipelineDesc& desc);
        std::shared_ptr<VulkanPipeline> GetOrCreateComputePipeline(const ComputePipelineDesc& desc);
        void ReloadAllShaders();

        void SetBindlessLayout(VkDescriptorSetLayout layout) noexcept { bindlessLayout = layout; }
        ENGINE_NODISCARD VkDescriptorSetLayout GetBindlessLayout() const noexcept { return bindlessLayout; }
        ENGINE_NODISCARD VkPipelineCache GetVkPipelineCache() const noexcept { return pipelineCache; }

    private:
        uint64_t HashGraphicsDesc(const GraphicsPipelineDesc& desc) const;
        uint64_t HashComputeDesc(const ComputePipelineDesc& desc) const;

        VkShaderModule CreateShaderModule(const std::string& path) const;
        static std::vector<char> ReadShaderFile(const std::string& path);

        VulkanDevice& device;
        VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        mutable std::mutex cacheMutex;
        std::unordered_map<uint64_t, std::shared_ptr<VulkanPipeline>> pipelineMap;
        std::unordered_map<uint64_t, GraphicsPipelineDesc> graphicsDescs;
        std::unordered_map<uint64_t, ComputePipelineDesc> computeDescs;
    };

} // namespace Engine


