#include "ResourceHeap.h"

namespace Engine
{
    ResourceHeap::ResourceHeap(Device& device, uint32_t maxTextures) : device(device), maxDescriptors(maxTextures)
    {
        slots.resize(maxDescriptors);

        freeIndices.reserve(maxDescriptors);
        for (uint32_t i = maxDescriptors; i > 0; --i)
        {
            freeIndices.push_back(i - 1);
        }

        pendingWrites.reserve(maxDescriptors/10);

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = maxDescriptors;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("ResourceHeap: Failed to create bindless descriptor pool");
        }

        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = 0;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding.descriptorCount = maxDescriptors;
        layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = 1;
        bindingFlagsInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;

        if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &globalDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("ResourceHeap: Failed to create bindless set layout");
        }

        uint32_t variableCounts[] = { maxDescriptors };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{};
        variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableCountInfo.descriptorSetCount = 1;
        variableCountInfo.pDescriptorCounts = variableCounts;

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.pNext = &variableCountInfo;
        setAllocInfo.descriptorPool = globalDescriptorPool;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &globalDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device.getDevice(), &setAllocInfo, &globalDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("ResourceHeap: Failed to allocate bindless descriptor set");
        }
    }

    ResourceHeap::~ResourceHeap()
    {
        if (globalDescriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.getDevice(), globalDescriptorSetLayout, nullptr);
        if (globalDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.getDevice(), globalDescriptorPool, nullptr);
    }

    ResourceHeap::TextureHandle ResourceHeap::registerTexture(VkDescriptorImageInfo imageInfo)
    {
        std::lock_guard<std::mutex> lock(heapMutex);

        if (imageInfo.imageView == VK_NULL_HANDLE || imageInfo.sampler == VK_NULL_HANDLE || imageInfo.imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            throw std::runtime_error("ResourceHeap: Attempted to register texture or shader invalid");
        }

        if (freeIndices.empty())
        {
            throw std::runtime_error("ResourceHeap: Bindless texture array is out of available slots!");
        }

        uint32_t allocatedIndex = freeIndices.back();
        freeIndices.pop_back();

        TextureHandle handle{};
        handle.index = allocatedIndex;
        handle.generation = slots[allocatedIndex].generation;
        slots[allocatedIndex].allocated = true;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = globalDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = allocatedIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        PendingWrite pending{};
        pending.dstArrayElement = allocatedIndex;
        pending.imageInfo = imageInfo;
        pendingWrites.push_back(pending);
        hasPendingWrites = true;

        return handle;
    }

    void ResourceHeap::freeTexture(TextureHandle& handle)
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        assert(handle.index < maxDescriptors && "ResourceHeap: Attempted to free an out-of-bounds index");
        assert(handle.generation == slots[handle.index].generation && "ResourceHeap: Attempted to free an out-of-bounds index");
        assert(slots[handle.index].allocated == true && "ResourceHeap: Attempted to free an out-of-bounds index");

        slots[handle.index].allocated = false;
        slots[handle.index].generation++;
        freeIndices.push_back(handle.index);
    }

    void ResourceHeap::flushPendingUpdates()
    {
        std::lock_guard<std::mutex> lock(heapMutex);

        if (!hasPendingWrites) return;

        std::vector<VkWriteDescriptorSet> writes(pendingWrites.size());

        for (uint32_t i = 0; i < writes.size(); ++i)
        {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = globalDescriptorSet;
            writes[i].dstBinding = 0;
            writes[i].dstArrayElement = pendingWrites[i].dstArrayElement;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &pendingWrites[i].imageInfo;
        }

        vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        pendingWrites.clear();
        hasPendingWrites = false;
    }

    bool ResourceHeap::isSlotAllocated(TextureHandle& handle) const
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        if (handle.index >= maxDescriptors || handle.generation != slots[handle.index].generation)
        {
            return false;
        }

        return slots[handle.index].allocated;
    }
}

