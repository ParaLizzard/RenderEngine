#include "Vulkan/ResourceHeap.h"

#include <array>

#include "Renderer/Renderer.h"
#include "AssetSystem/Texture.h"

namespace Engine {


    ResourceHeap::ResourceHeap(Device &device, uint32_t maxTextures): device(device), maxDescriptors(maxTextures)
    {
        slots.resize(maxDescriptors);

        freeIndices.reserve(maxDescriptors);
        for (uint32_t i = maxDescriptors; i > 0; --i) {
            freeIndices.push_back(i - 1);
        }

        pendingWrites.reserve(maxDescriptors / 10);

        std::array<VkDescriptorPoolSize, 3> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 1 * Config::MAX_FRAMES_IN_FLIGHT;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 1 * Config::MAX_FRAMES_IN_FLIGHT;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = (maxDescriptors + 3) * Config::MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &globalDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("ResourceHeap: Failed to create bindless descriptor pool");
        }

        std::array<VkDescriptorSetLayoutBinding, 6> bindings {};

        // Binding 0: Material SSBO
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 1: Scene UBO
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 2: Irradiance Map (Cube)
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 3: Prefilter Map (Cube)
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 4: BRDF LUT (2D)
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 5: Variable Textures (Must be last!)
        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[5].descriptorCount = maxDescriptors;
        bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorBindingFlags, 6> bindingFlags {};
        bindingFlags[0] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        bindingFlags[1] = 0;
        bindingFlags[2] = 0;
        bindingFlags[3] = 0;
        bindingFlags[4] = 0;
        bindingFlags[5] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;


        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo {};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &globalDescriptorSetLayout) !=
            VK_SUCCESS) {
            throw std::runtime_error("ResourceHeap: Failed to create bindless set layout");
        }

        std::vector<uint32_t> variableCounts(Config::MAX_FRAMES_IN_FLIGHT, maxDescriptors);
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo {};
        variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableCountInfo.descriptorSetCount = Config::MAX_FRAMES_IN_FLIGHT;
        variableCountInfo.pDescriptorCounts = variableCounts.data();

        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = &variableCountInfo;
        allocInfo.descriptorPool = globalDescriptorPool;
        allocInfo.descriptorSetCount = Config::MAX_FRAMES_IN_FLIGHT;
        std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, globalDescriptorSetLayout);
        allocInfo.pSetLayouts = layouts.data();

        globalDescriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device.getDevice(), &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("ResourceHeap: failed to allocate descriptor sets!");
        }

        fallbackWhiteTex = std::make_unique<Texture2D>();
        fallbackFlatNormalTex = std::make_unique<Texture2D>();

        uint8_t white[4] = {255, 255, 255, 255};
        uint8_t flatNormal[4] = {128, 128, 255, 255};

        fallbackWhiteTex->fromBuffer(white, 4, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, &device, *this);
        fallbackFlatNormalTex->fromBuffer(flatNormal, 4, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, &device, *this);

        fallbackWhiteSlot = fallbackWhiteTex->heapHandle.index;
        fallbackFlatNormalSlot = fallbackFlatNormalTex->heapHandle.index;

        flushPendingUpdates();
    }

    void ResourceHeap::writeSceneUboDescriptor(VkDescriptorBufferInfo bufInfo, uint32_t frameIdx)
    {
        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = globalDescriptorSets[frameIdx];
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
    }

    ResourceHeap::~ResourceHeap()
    {
        if (fallbackWhiteTex)
            fallbackWhiteTex->destroy();
        if (fallbackFlatNormalTex)
            fallbackFlatNormalTex->destroy();

        if (globalDescriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.getDevice(), globalDescriptorSetLayout, nullptr);
        if (globalDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.getDevice(), globalDescriptorPool, nullptr);
    }

    ResourceHeap::TextureHandle ResourceHeap::registerTexture(VkDescriptorImageInfo imageInfo)
    {
        std::lock_guard<std::mutex> lock(heapMutex);

        if (imageInfo.imageView == VK_NULL_HANDLE || imageInfo.sampler == VK_NULL_HANDLE ||
            imageInfo.imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            throw std::runtime_error("ResourceHeap: Attempted to register texture or shader invalid");
        }

        if (freeIndices.empty()) {
            throw std::runtime_error("ResourceHeap: Bindless texture array is out of available slots!");
        }

        uint32_t allocatedIndex = freeIndices.back();
        freeIndices.pop_back();

        TextureHandle handle {};
        handle.index = allocatedIndex;
        handle.generation = slots[allocatedIndex].generation;
        slots[allocatedIndex].allocated = true;

        PendingWrite pending {};
        pending.dstArrayElement = allocatedIndex;
        pending.imageInfo = imageInfo;
        pendingWrites.push_back(pending);
        hasPendingWrites = true;

        return handle;
    }

    void ResourceHeap::freeTexture(TextureHandle &handle)
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        assert(handle.index < maxDescriptors && "ResourceHeap: Attempted to free an out-of-bounds index");
        assert(handle.generation == slots[handle.index].generation &&
               "ResourceHeap: Attempted to free an out-of-bounds index");
        assert(slots[handle.index].allocated == true && "ResourceHeap: Attempted to free an out-of-bounds index");

        slots[handle.index].allocated = false;
        slots[handle.index].generation++;
        freeIndices.push_back(handle.index);
    }

    void ResourceHeap::flushPendingUpdates()
    {
        std::lock_guard<std::mutex> lock(heapMutex);

        if (!hasPendingWrites)
            return;

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(pendingWrites.size() * Config::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t f = 0; f < Config::MAX_FRAMES_IN_FLIGHT; ++f) {
            for (const auto &pending: pendingWrites) {
                VkWriteDescriptorSet write {};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = globalDescriptorSets[f];
                write.dstBinding = 5;
                write.dstArrayElement = pending.dstArrayElement;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo = &pending.imageInfo;
                writes.push_back(write);
            }
        }

        vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        pendingWrites.clear();
        hasPendingWrites = false;
    }

    bool ResourceHeap::isSlotAllocated(TextureHandle &handle) const
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        if (handle.index >= maxDescriptors || handle.generation != slots[handle.index].generation) {
            return false;
        }

        return slots[handle.index].allocated;
    }

    uint32_t ResourceHeap::pushMaterial(const MaterialData &mat)
    {
        std::lock_guard<std::mutex> lock(heapMutex);
        uint32_t id = static_cast<uint32_t>(materials.size());
        materials.push_back(mat);
        return id;
    }

    void ResourceHeap::uploadMaterialBuffer(uint32_t currentFrame)
    {
        if (materials.empty())
            return;

        VkDeviceSize bufferSize = sizeof(MaterialData) * materials.size();

        if (materialBuffers.size() < Config::MAX_FRAMES_IN_FLIGHT) {
            materialBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        }

        if (!materialBuffers[currentFrame] || materialBuffers[currentFrame]->getBufferSize() < bufferSize) {
            materialBuffers[currentFrame] =
                std::make_unique<Buffer>(device,
                                         sizeof(MaterialData),
                                         static_cast<uint32_t>(materials.size()),
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                         0);
        }

        materialBuffers[currentFrame]->writeToBuffer(materials.data(), bufferSize, 0);
        materialBuffers[currentFrame]->flush(bufferSize, 0);
    }

    VkDescriptorBufferInfo ResourceHeap::getMaterialBufferInfo(uint32_t currentFrame) const
    {
        assert(materialBuffers.size() > currentFrame && materialBuffers[currentFrame] &&
               "Material buffer not uploaded yet");
        return materialBuffers[currentFrame]->descriptorInfo(sizeof(MaterialData) * materials.size(), 0);
    }

    void ResourceHeap::update(uint32_t currentFrame)
    {
        if (materialFramesToUpdate > 0) {
            uploadMaterialBuffer(currentFrame);
            writeMaterialDescriptor(currentFrame);
            materialFramesToUpdate--;
        }
    }

    void ResourceHeap::writeMaterialDescriptorAllFrames()
    {
        std::vector<VkDescriptorBufferInfo> matBufInfos(Config::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkWriteDescriptorSet> writes;

        for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            if (materialBuffers.size() <= i || !materialBuffers[i])
                continue;

            matBufInfos[i] = materialBuffers[i]->descriptorInfo(sizeof(MaterialData) * materials.size(), 0);

            VkWriteDescriptorSet write {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = globalDescriptorSets[i];
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &matBufInfos[i];
            writes.push_back(write);
        }

        if (!writes.empty()) {
            vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void ResourceHeap::writeMaterialDescriptor(uint32_t currentFrame)
    {
        if (materialBuffers.size() <= currentFrame || !materialBuffers[currentFrame])
            return;

        VkDescriptorBufferInfo matBufInfo =
            materialBuffers[currentFrame]->descriptorInfo(sizeof(MaterialData) * materials.size(), 0);

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = globalDescriptorSets[currentFrame];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &matBufInfo;

        vkUpdateDescriptorSets(device.getDevice(), 1, &write, 0, nullptr);
    }

    void ResourceHeap::writeIBLDescriptors(VkDescriptorImageInfo irradianceInfo,
                                           VkDescriptorImageInfo prefilterInfo,
                                           VkDescriptorImageInfo brdfLutInfo)
    {
        std::vector<VkWriteDescriptorSet> writes;
        for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            VkWriteDescriptorSet w0 {};
            w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w0.dstSet = globalDescriptorSets[i];
            w0.dstBinding = 2;
            w0.dstArrayElement = 0;
            w0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w0.descriptorCount = 1;
            w0.pImageInfo = &irradianceInfo;
            writes.push_back(w0);

            VkWriteDescriptorSet w1 {};
            w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w1.dstSet = globalDescriptorSets[i];
            w1.dstBinding = 3;
            w1.dstArrayElement = 0;
            w1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w1.descriptorCount = 1;
            w1.pImageInfo = &prefilterInfo;
            writes.push_back(w1);

            VkWriteDescriptorSet w2 {};
            w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w2.dstSet = globalDescriptorSets[i];
            w2.dstBinding = 4;
            w2.dstArrayElement = 0;
            w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w2.descriptorCount = 1;
            w2.pImageInfo = &brdfLutInfo;
            writes.push_back(w2);
        }

        vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

} // namespace Engine
