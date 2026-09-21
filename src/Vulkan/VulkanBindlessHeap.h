#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <mutex>
#include <array>
#include <unordered_map>

#include "Core/CoreDefines.h"
#include "Core/EngineConstants.h"
#include "Vulkan/VulkanSamplers.h"

namespace Engine {
    class VulkanDevice;

    struct BindlessTextureHandle {
        uint32_t slot = 0xFFFFFFFF;
        uint32_t generation = 0;

        ENGINE_NODISCARD bool IsValid() const noexcept { return slot != 0xFFFFFFFF; }
        bool operator==(const BindlessTextureHandle& o) const noexcept {
            return slot == o.slot && generation == o.generation;
        }
    };

    class VulkanBindlessHeap {
    public:
        explicit VulkanBindlessHeap(VulkanDevice& device, uint32_t maxTextures = Constants::MAX_TEXTURES);
        ~VulkanBindlessHeap();

        BindlessTextureHandle RegisterTexture(VkImageView imageView, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bool bImmediate = false);
        void UnregisterTexture(BindlessTextureHandle handle);

        ENGINE_NODISCARD VkDescriptorSet GetDescriptorSet() const noexcept { return descriptorSet; }
        ENGINE_NODISCARD VkDescriptorSetLayout GetDescriptorSetLayout() const noexcept { return descriptorSetLayout; }
        ENGINE_NODISCARD VkSampler GetSampler(SamplerType type) const noexcept { return samplers.GetSampler(type); }
        ENGINE_NODISCARD const VulkanSamplers& GetSamplers() const noexcept { return samplers; }

        void FlushPendingUpdates();

    private:
        void CreateLayoutAndPool();

        VulkanDevice& device;
        uint32_t capacity;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        VulkanSamplers samplers;
        std::vector<uint32_t> freeSlots;
        std::unordered_map<uint32_t, uint32_t> slotGenerations;
        std::vector<VkWriteDescriptorSet> pendingWrites;
        std::mutex mutex;
    };
}