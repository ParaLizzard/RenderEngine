
#include "VulkanBindlessHeap.h"

#include <ranges>
#include <algorithm>

#include "VulkanDevice.h"

namespace Engine
{
    VulkanBindlessHeap::VulkanBindlessHeap(VulkanDevice &device, uint32_t maxTextures)
        : device(device), capacity(maxTextures), samplers(device)
    {
        auto r = std::views::iota(0u, maxTextures);
        freeSlots.assign(r.begin(), r.end());

        CreateLayoutAndPool();
    }

    VulkanBindlessHeap::~VulkanBindlessHeap()
    {
        device.WaitIdle();

        vkDestroyDescriptorPool(device.GetHandle(), descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device.GetHandle(), descriptorSetLayout, nullptr);
    }

    BindlessTextureHandle VulkanBindlessHeap::RegisterTexture(VkImageView imageView, VkImageLayout layout, bool bImmediate)
    {
        std::lock_guard<std::mutex> lock(mutex);

        ENGINE_VERIFY(freeSlots.empty(), "ImmutableSamplers was empty");

        uint32_t slot = freeSlots.back();
        freeSlots.pop_back();

        BindlessTextureHandle handle{slot, slotGenerations[slot]};

        VkDescriptorImageInfo descInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = imageView,
        .imageLayout = layout
        };

        VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = slot,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &descInfo
        };

        if (bImmediate) {
            vkUpdateDescriptorSets(device.GetHandle(), 1, &write, 0, nullptr);
        } else {
            pendingWrites.push_back(write);
        }

        return handle;
    }

    void VulkanBindlessHeap::UnregisterTexture(BindlessTextureHandle handle)
    {
        if (!handle.IsValid()) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (handle.generation != slotGenerations[handle.slot]) {
            LOG_WARN("BindlessHeap", "Invalid texture handle: %d", handle.generation);
            return;
        }

        slotGenerations[handle.slot]++;

        freeSlots.push_back(handle.slot);
    }

    void VulkanBindlessHeap::FlushPendingUpdates()
    {
        vkUpdateDescriptorSets(device.GetHandle(), pendingWrites.size(), pendingWrites.data(), 0, nullptr);
        pendingWrites.clear();
    }

    void VulkanBindlessHeap::CreateLayoutAndPool()
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        binding.descriptorCount = capacity;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagCreateInfo{};
        flagCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagCreateInfo.bindingCount = 1;
        flagCreateInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &flagCreateInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        VkResult res = vkCreateDescriptorSetLayout(device.GetHandle(), &layoutInfo, nullptr, &descriptorSetLayout);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create descriptor set layout!");
        device.SetObjectName(descriptorSetLayout, "BindlessHeap_DescriptorSetLayout");

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSize.descriptorCount = capacity;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        res = vkCreateDescriptorPool(device.GetHandle(), &poolInfo, nullptr, &descriptorPool);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to create descriptor pool!");
        device.SetObjectName(descriptorPool, "BindlessHeap_DescriptorPool");

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        res = vkAllocateDescriptorSets(device.GetHandle(), &allocInfo, &descriptorSet);
        ENGINE_VERIFY(res == VK_SUCCESS, "Failed to allocate descriptor set!");
        device.SetObjectName(descriptorSet, "BindlessHeap_DescriptorSet");
    }
} // Engine