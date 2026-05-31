#include "ResourceHeap.h"

#include <array>

#include "Scene/Texture.h"

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

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 1;
        poolSizes[2].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = maxDescriptors + 3;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();

        if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("ResourceHeap: Failed to create bindless descriptor pool");
        }

        std::array<VkDescriptorSetLayoutBinding, 6> bindings{};

        // Binding 0: Material SSBO
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 1: Scene UBO
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 2: Irradiance Map (Cube)
        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 3: Prefilter Map (Cube)
        bindings[3].binding         = 3;
        bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 4: BRDF LUT (2D)
        bindings[4].binding         = 4;
        bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 5: Variable Textures (Must be last!)
        bindings[5].binding         = 5;
        bindings[5].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[5].descriptorCount = maxDescriptors;
        bindings[5].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorBindingFlags, 6> bindingFlags{};
        bindingFlags[0] = 0;
        bindingFlags[1] = 0;
        bindingFlags[2] = 0;
        bindingFlags[3] = 0;
        bindingFlags[4] = 0;
        bindingFlags[5] =
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT   |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;


        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext        = &bindingFlagsInfo;
        layoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();

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

        fallbackWhiteTex     = std::make_unique<Texture2D>();
        fallbackFlatNormalTex = std::make_unique<Texture2D>();

        uint8_t white[4]      = {255, 255, 255, 255};
        uint8_t flatNormal[4] = {128, 128, 255, 255};

        fallbackWhiteTex->fromBuffer(white, 4, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, &device, *this);
        fallbackFlatNormalTex->fromBuffer(flatNormal, 4, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, &device, *this);

        fallbackWhiteSlot      = fallbackWhiteTex->heapHandle.index;
        fallbackFlatNormalSlot = fallbackFlatNormalTex->heapHandle.index;

        flushPendingUpdates();
    }

    void ResourceHeap::writeSceneUboDescriptor(VkDescriptorBufferInfo bufInfo)
    {
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = globalDescriptorSet;
        write.dstBinding      = 1;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
    }

    ResourceHeap::~ResourceHeap()
    {
        if (fallbackWhiteTex)     fallbackWhiteTex->destroy();
        if (fallbackFlatNormalTex) fallbackFlatNormalTex->destroy();

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
        write.dstBinding = 5;
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
            writes[i].dstBinding = 5;
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

    uint32_t ResourceHeap::pushMaterial(const MaterialData& mat)
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        uint32_t id = static_cast<uint32_t>(materials.size());
        materials.push_back(mat);
        return id;
    }

    void ResourceHeap::uploadMaterialBuffer()
    {
        if (materials.empty()) return;

        VkDeviceSize bufferSize = sizeof(MaterialData) * materials.size();

        if (!materialBuffer || materialBuffer->getBufferSize() < bufferSize)
        {
            materialBuffer = std::make_unique<Buffer>(
                device,
                sizeof(MaterialData),
                static_cast<uint32_t>(materials.size()),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                0
            );
        }

        materialBuffer->writeToBuffer(materials.data(), bufferSize, 0);
        materialBuffer->flush(bufferSize, 0);
    }

    VkDescriptorBufferInfo ResourceHeap::getMaterialBufferInfo() const
    {
        assert(materialBuffer && "Material buffer not uploaded yet");
        return materialBuffer->descriptorInfo(
            sizeof(MaterialData) * materials.size(), 0);
    }

    void ResourceHeap::writeMaterialDescriptor()
    {
        assert(materialBuffer && "Call uploadMaterialBuffer first");

        VkDescriptorBufferInfo bufInfo = getMaterialBufferInfo();

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = globalDescriptorSet;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
    }

    void ResourceHeap::writeIBLDescriptors(VkDescriptorImageInfo irradianceInfo, VkDescriptorImageInfo prefilterInfo, VkDescriptorImageInfo brdfLutInfo)
    {
        std::array<VkWriteDescriptorSet, 3> writes{};

        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = globalDescriptorSet;
        writes[0].dstBinding      = 2;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo      = &irradianceInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = globalDescriptorSet;
        writes[1].dstBinding      = 3;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &prefilterInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = globalDescriptorSet;
        writes[2].dstBinding      = 4;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo      = &brdfLutInfo;

        vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

}

