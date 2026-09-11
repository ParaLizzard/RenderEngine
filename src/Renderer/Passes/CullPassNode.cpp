#include "Renderer/Passes/CullPassNode.h"
#include "Core/Assert.h"
#include "Renderer/RenderSettings.h"
#include "Vulkan/Buffer.h"
#include <array>
#include <vector>

#include "Core/EngineConstants.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    CullPassNode::CullPassNode(VulkanDevice &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, uint32_t phase, CullPassNode *parentCullPass):
        RenderPassNode(phase == 0 ? "Cull Pass Phase 1" : "Cull Pass Phase 2"), device(device), megaBuffer(megaBuffer), renderer(renderer), resourceHeap(resourceHeap), phase(phase), parentCullPass(parentCullPass)
    {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 16.0f;
        vkCreateSampler(device.GetHandle(), &samplerInfo, nullptr, &hizSampler);

        objectDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        if (device.IsMeshShaderSupported()) {
            stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
        }

        std::array<VkDescriptorSetLayoutBinding, 11> ssboBindings {};
        ssboBindings[0].binding = 0;
        ssboBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[0].descriptorCount = 1;
        ssboBindings[0].stageFlags = stageFlags;

        ssboBindings[1].binding = 1;
        ssboBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[1].descriptorCount = 1;
        ssboBindings[1].stageFlags = stageFlags;

        ssboBindings[2].binding = 2;
        ssboBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[2].descriptorCount = 1;
        ssboBindings[2].stageFlags = stageFlags;

        ssboBindings[3].binding = 3;
        ssboBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[3].descriptorCount = 1;
        ssboBindings[3].stageFlags = stageFlags;

        ssboBindings[4].binding = 4;
        ssboBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[4].descriptorCount = 1;
        ssboBindings[4].stageFlags = stageFlags;

        ssboBindings[5].binding = 5;
        ssboBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[5].descriptorCount = 1;
        ssboBindings[5].stageFlags = stageFlags;

        ssboBindings[6].binding = 6;
        ssboBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[6].descriptorCount = 1;
        ssboBindings[6].stageFlags = stageFlags;

        ssboBindings[7].binding = 7;
        ssboBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[7].descriptorCount = 1;
        ssboBindings[7].stageFlags = stageFlags;

        ssboBindings[8].binding = 8;
        ssboBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssboBindings[8].descriptorCount = 1;
        ssboBindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | (device.IsMeshShaderSupported() ? VK_SHADER_STAGE_TASK_BIT_EXT : 0);

        ssboBindings[9].binding = 9;
        ssboBindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[9].descriptorCount = 1;
        ssboBindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        ssboBindings[10].binding = 10;
        ssboBindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[10].descriptorCount = 1;
        ssboBindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.GetHandle(), &layoutInfo, nullptr, &objectSetLayout);

        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Constants::MAX_FRAMES_IN_FLIGHT * 10 * 2;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = Constants::MAX_FRAMES_IN_FLIGHT * 1 * 2;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = Constants::MAX_FRAMES_IN_FLIGHT * 2;
        vkCreateDescriptorPool(device.GetHandle(), &poolInfo, nullptr, &objectDescriptorPool);

        objectDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        maskedObjectDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        gpuDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuVisibleObjectBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        compactedIndexBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        singleIndirectCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuDrawCountBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuIndirectCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        triangleDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        visibleMeshletBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        taskWorkgroupBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        taskDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        gpuCandidateDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuCandidateObjectBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuMaskedCandidateDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuMaskedCandidateObjectBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        gpuMaskedDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuMaskedVisibleObjectBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        maskedCompactedIndexBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        maskedSingleIndirectCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        maskedTriangleDispatchCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        maskedVisibleMeshletBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuMaskedIndirectCommandBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        gpuMaskedDrawCountBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            gpuDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuVisibleObjectBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Constants::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            compactedIndexBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            singleIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            triangleDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            visibleMeshletBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Constants::MAX_SCENE_OBJECTS * 100,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuMaskedDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuMaskedVisibleObjectBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Constants::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            maskedCompactedIndexBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            maskedSingleIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            maskedTriangleDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            maskedVisibleMeshletBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Constants::MAX_SCENE_OBJECTS * 100,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            taskWorkgroupBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Constants::MAX_SCENE_OBJECTS * 10,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            taskDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuDrawCountBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         Constants::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            gpuMaskedIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         Constants::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            if (phase == 0) {
                gpuCandidateDispatchCommandBuffers[i] =
                    std::make_unique<Buffer>(device,
                                             sizeof(VkDispatchIndirectCommand),
                                             1,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             0);

                gpuCandidateObjectBuffers[i] =
                    std::make_unique<Buffer>(device,
                                             sizeof(uint32_t),
                                             Constants::MAX_SCENE_OBJECTS,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             0);

                gpuMaskedCandidateDispatchCommandBuffers[i] =
                    std::make_unique<Buffer>(device,
                                             sizeof(VkDispatchIndirectCommand),
                                             1,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             0);

                gpuMaskedCandidateObjectBuffers[i] =
                    std::make_unique<Buffer>(device,
                                             sizeof(uint32_t),
                                             Constants::MAX_SCENE_OBJECTS,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             0);
            }

            VkDescriptorSetAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            ENGINE_VERIFY(vkAllocateDescriptorSets(device.GetHandle(), &allocInfo, &objectDescriptorSets[i]) == VK_SUCCESS,
                "Failed to allocate object descriptor sets in CullPassNode");
            ENGINE_VERIFY(vkAllocateDescriptorSets(device.GetHandle(), &allocInfo, &maskedObjectDescriptorSets[i]) == VK_SUCCESS,
                "Failed to allocate masked object descriptor sets in CullPassNode");

            VkDescriptorBufferInfo dispatchInfo = gpuDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleObjInfo = gpuVisibleObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo singleIndirectInfo = singleIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo = compactedIndexBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleMeshletsInfo = visibleMeshletBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo triangleDispatchInfo = triangleDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskWorkgroupInfo = taskWorkgroupBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskDispatchInfo = taskDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            VkDescriptorBufferInfo candDispatchInfo = (phase == 0)
                ? gpuCandidateDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0)
                : parentCullPass->gpuCandidateDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            VkDescriptorBufferInfo candObjInfo = (phase == 0)
                ? gpuCandidateObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0)
                : parentCullPass->gpuCandidateObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 10> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = objectDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &dispatchInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = objectDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &visibleObjInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = objectDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &singleIndirectInfo;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = objectDescriptorSets[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &compactedInfo;

            descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[4].dstSet = objectDescriptorSets[i];
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pBufferInfo = &visibleMeshletsInfo;

            descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[5].dstSet = objectDescriptorSets[i];
            descriptorWrites[5].dstBinding = 5;
            descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[5].descriptorCount = 1;
            descriptorWrites[5].pBufferInfo = &triangleDispatchInfo;

            descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[6].dstSet = objectDescriptorSets[i];
            descriptorWrites[6].dstBinding = 6;
            descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[6].descriptorCount = 1;
            descriptorWrites[6].pBufferInfo = &taskWorkgroupInfo;

            descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[7].dstSet = objectDescriptorSets[i];
            descriptorWrites[7].dstBinding = 7;
            descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[7].descriptorCount = 1;
            descriptorWrites[7].pBufferInfo = &taskDispatchInfo;

            descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[8].dstSet = objectDescriptorSets[i];
            descriptorWrites[8].dstBinding = 9;
            descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[8].descriptorCount = 1;
            descriptorWrites[8].pBufferInfo = &candDispatchInfo;

            descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[9].dstSet = objectDescriptorSets[i];
            descriptorWrites[9].dstBinding = 10;
            descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[9].descriptorCount = 1;
            descriptorWrites[9].pBufferInfo = &candObjInfo;

            vkUpdateDescriptorSets(device.GetHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

            VkDescriptorBufferInfo maskedDispatchInfo = gpuMaskedDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedVisibleObjInfo = gpuMaskedVisibleObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedSingleIndirectInfo = maskedSingleIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedCompactedInfo = maskedCompactedIndexBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedVisibleMeshletsInfo = maskedVisibleMeshletBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedTriangleDispatchInfo = maskedTriangleDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            VkDescriptorBufferInfo maskedCandDispatchInfo = (phase == 0)
                ? gpuMaskedCandidateDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0)
                : parentCullPass->gpuMaskedCandidateDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            VkDescriptorBufferInfo maskedCandObjInfo = (phase == 0)
                ? gpuMaskedCandidateObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0)
                : parentCullPass->gpuMaskedCandidateObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 10> maskedDescriptorWrites = descriptorWrites;
            for (auto &w : maskedDescriptorWrites) {
                w.dstSet = maskedObjectDescriptorSets[i];
            }
            maskedDescriptorWrites[0].pBufferInfo = &maskedDispatchInfo;
            maskedDescriptorWrites[1].pBufferInfo = &maskedVisibleObjInfo;
            maskedDescriptorWrites[2].pBufferInfo = &maskedSingleIndirectInfo;
            maskedDescriptorWrites[3].pBufferInfo = &maskedCompactedInfo;
            maskedDescriptorWrites[4].pBufferInfo = &maskedVisibleMeshletsInfo;
            maskedDescriptorWrites[5].pBufferInfo = &maskedTriangleDispatchInfo;
            maskedDescriptorWrites[8].pBufferInfo = &maskedCandDispatchInfo;
            maskedDescriptorWrites[9].pBufferInfo = &maskedCandObjInfo;

            vkUpdateDescriptorSets(device.GetHandle(), static_cast<uint32_t>(maskedDescriptorWrites.size()), maskedDescriptorWrites.data(), 0, nullptr);
        }

        createPipeline();
    }

    CullPassNode::~CullPassNode()
    {
        if (hizSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), hizSampler, nullptr);
        if (objectDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.GetHandle(), objectDescriptorPool, nullptr);
        if (objectSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.GetHandle(), objectSetLayout, nullptr);
        if (objectCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), objectCullPipeline, nullptr);
        if (taskSubmitPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), taskSubmitPipeline, nullptr);
        if (meshletCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), meshletCullPipeline, nullptr);
        if (triangleCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), triangleCullPipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.GetHandle(), computePipelineLayout, nullptr);
    }

    void CullPassNode::createPipeline()
    {
        auto objCode = ShaderUtils::readFile("shaders/object_cull.comp.spv");
        VkShaderModule objModule = ShaderUtils::createShaderModule(device.GetHandle(), objCode);

        auto taskSubmitCode = ShaderUtils::readFile("shaders/task_submit.comp.spv");
        VkShaderModule taskSubmitModule = ShaderUtils::createShaderModule(device.GetHandle(), taskSubmitCode);

        auto meshletCode = ShaderUtils::readFile("shaders/meshlet_cull.comp.spv");
        VkShaderModule meshletModule = ShaderUtils::createShaderModule(device.GetHandle(), meshletCode);

        auto triangleCode = ShaderUtils::readFile("shaders/triangle_cull.comp.spv");
        VkShaderModule triangleModule = ShaderUtils::createShaderModule(device.GetHandle(), triangleCode);

        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ComputePushConstants);

        VkDescriptorSetLayout layouts[] = {resourceHeap.getDescriptorSetLayout(), objectSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        ENGINE_VERIFY(vkCreatePipelineLayout(device.GetHandle(), &pipelineLayoutInfo, nullptr, &computePipelineLayout) == VK_SUCCESS,
            "CullPassNode: failed to create compute pipeline layout");

        uint32_t workgroupSize = Constants::CULL_WORKGROUP_SIZE;
        VkSpecializationMapEntry specEntry {};
        specEntry.constantID = 0;
        specEntry.offset = 0;
        specEntry.size = sizeof(uint32_t);

        VkSpecializationInfo specInfo {};
        specInfo.mapEntryCount = 1;
        specInfo.pMapEntries = &specEntry;
        specInfo.dataSize = sizeof(uint32_t);
        specInfo.pData = &workgroupSize;

        VkPipelineShaderStageCreateInfo objStageInfo {};
        objStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        objStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        objStageInfo.module = objModule;
        objStageInfo.pName = "main";
        objStageInfo.pSpecializationInfo = &specInfo;

        VkPipelineShaderStageCreateInfo taskSubmitStageInfo {};
        taskSubmitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        taskSubmitStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        taskSubmitStageInfo.module = taskSubmitModule;
        taskSubmitStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo meshletStageInfo {};
        meshletStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        meshletStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        meshletStageInfo.module = meshletModule;
        meshletStageInfo.pName = "main";
        meshletStageInfo.pSpecializationInfo = &specInfo;

        VkPipelineShaderStageCreateInfo triangleStageInfo {};
        triangleStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        triangleStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        triangleStageInfo.module = triangleModule;
        triangleStageInfo.pName = "main";

        VkComputePipelineCreateInfo objPipelineInfo {};
        objPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        objPipelineInfo.layout = computePipelineLayout;
        objPipelineInfo.stage = objStageInfo;

        ENGINE_VERIFY(vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &objPipelineInfo, nullptr, &objectCullPipeline) == VK_SUCCESS,
            "CullPassNode: failed to create object cull compute pipeline");

        VkComputePipelineCreateInfo taskSubmitPipelineInfo {};
        taskSubmitPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        taskSubmitPipelineInfo.layout = computePipelineLayout;
        taskSubmitPipelineInfo.stage = taskSubmitStageInfo;

        ENGINE_VERIFY(vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &taskSubmitPipelineInfo, nullptr, &taskSubmitPipeline) == VK_SUCCESS,
            "CullPassNode: failed to create task submit compute pipeline");

        VkComputePipelineCreateInfo meshletPipelineInfo {};
        meshletPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        meshletPipelineInfo.layout = computePipelineLayout;
        meshletPipelineInfo.stage = meshletStageInfo;

        ENGINE_VERIFY(vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &meshletPipelineInfo, nullptr, &meshletCullPipeline) == VK_SUCCESS,
            "CullPassNode: failed to create meshlet cull compute pipeline");

        VkComputePipelineCreateInfo trianglePipelineInfo {};
        trianglePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        trianglePipelineInfo.layout = computePipelineLayout;
        trianglePipelineInfo.stage = triangleStageInfo;

        ENGINE_VERIFY(vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &trianglePipelineInfo, nullptr, &triangleCullPipeline) == VK_SUCCESS,
            "CullPassNode: failed to create triangle cull compute pipeline");

        vkDestroyShaderModule(device.GetHandle(), objModule, nullptr);
        vkDestroyShaderModule(device.GetHandle(), taskSubmitModule, nullptr);
        vkDestroyShaderModule(device.GetHandle(), meshletModule, nullptr);
        vkDestroyShaderModule(device.GetHandle(), triangleModule, nullptr);
    }

    void CullPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        renderGraph.readImage("HiZImage",
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);

        if (!device.IsMeshShaderSupported()) {
            renderGraph.writeBuffer("CompactedIndexBuffer",
                                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.writeBuffer("SingleIndirectCommand",
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.writeBuffer("MaskedCompactedIndexBuffer",
                                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.writeBuffer("MaskedSingleIndirectCommand",
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_WRITE_BIT);
        }
    }

    void CullPassNode::registerResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        uint32_t currentFrame = frameInfo.frameIndex;
        graph.registerPhysicalBuffer("CompactedIndexBuffer",
                                     getCompactedIndexBuffer(currentFrame),
                                     Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t),
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT);

        graph.registerPhysicalBuffer("SingleIndirectCommand",
                                     getSingleIndirectCommandBuffer(currentFrame),
                                     sizeof(VkDrawIndirectCommand),
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT);

        graph.registerPhysicalBuffer("MaskedCompactedIndexBuffer",
                                     getMaskedCompactedIndexBuffer(currentFrame),
                                     Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t),
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT);

        graph.registerPhysicalBuffer("MaskedSingleIndirectCommand",
                                     getMaskedSingleIndirectCommandBuffer(currentFrame),
                                     sizeof(VkDrawIndirectCommand),
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void CullPassNode::updateResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        uint32_t currentFrame = frameInfo.frameIndex;
        graph.updateBufferHandle("CompactedIndexBuffer",
                                 getCompactedIndexBuffer(currentFrame),
                                 Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t));

        graph.updateBufferHandle("SingleIndirectCommand",
                                 getSingleIndirectCommandBuffer(currentFrame),
                                 sizeof(VkDrawIndirectCommand));

        graph.updateBufferHandle("MaskedCompactedIndexBuffer",
                                 getMaskedCompactedIndexBuffer(currentFrame),
                                 Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t));

        graph.updateBufferHandle("MaskedSingleIndirectCommand",
                                 getMaskedSingleIndirectCommandBuffer(currentFrame),
                                 sizeof(VkDrawIndirectCommand));
    }

    void CullPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        glm::mat4 projection = frameInfo.camera->getProjection();
        glm::mat4 view = frameInfo.camera->getView();
        glm::mat4 viewProjection = projection * view;

        uint32_t currentFrame = renderer.getFrameIndex();
        if (sceneDirty) {
            opaqueDraws.clear();
            indirectCommandsArray.clear();
            maskedDraws.clear();
            maskedIndirectCommandsArray.clear();

            for (size_t i = 0; i < frameInfo.gameObjects->size(); i++) {
                const auto &obj = (*frameInfo.gameObjects)[i];
                if (obj.subMesh.indexCount == 0)
                    continue;

                VkDrawIndexedIndirectCommand cmdCommand{};
                cmdCommand.indexCount    = obj.subMesh.indexCount;
                cmdCommand.instanceCount = 1;
                cmdCommand.firstIndex    = obj.subMesh.firstIndex;
                cmdCommand.vertexOffset  = obj.subMesh.vertexOffset;
                cmdCommand.firstInstance = static_cast<uint32_t>(i);

                if (obj.alphaMode == AlphaMode::Opaque) {
                    opaqueDraws.push_back(&obj);
                    indirectCommandsArray.push_back(cmdCommand);
                } else if (obj.alphaMode == AlphaMode::Mask) {
                    maskedDraws.push_back(&obj);
                    maskedIndirectCommandsArray.push_back(cmdCommand);
                }
            }

            framesToUpdate = Constants::MAX_FRAMES_IN_FLIGHT;
            sceneDirty = false;
        }

        if (framesToUpdate > 0) {
            if (!indirectCommandsArray.empty()) {
                gpuIndirectCommandBuffers[currentFrame]->writeToBuffer(
                    indirectCommandsArray.data(),
                    indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand),
                    0);
                gpuIndirectCommandBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);
            }

            if (!maskedIndirectCommandsArray.empty()) {
                gpuMaskedIndirectCommandBuffers[currentFrame]->writeToBuffer(
                    maskedIndirectCommandsArray.data(),
                    maskedIndirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand),
                    0);
                gpuMaskedIndirectCommandBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);
            }

            framesToUpdate--;
        }

        uint32_t totalObjects = static_cast<uint32_t>(frameInfo.gameObjects->size());
        if (totalObjects > 0) {
            VkDispatchIndirectCommand dispatchCmd{0, 1, 1};
            vkCmdUpdateBuffer(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &dispatchCmd);
            vkCmdUpdateBuffer(cmd, gpuMaskedDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &dispatchCmd);

            std::vector<VkBufferMemoryBarrier2> transferBarriers;
            transferBarriers.push_back(VkUtils::bufferBarrier(
                gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            transferBarriers.push_back(VkUtils::bufferBarrier(
                gpuMaskedDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));

            if (phase == 0) {
                vkCmdUpdateBuffer(cmd, gpuCandidateDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &dispatchCmd);
                vkCmdUpdateBuffer(cmd, gpuMaskedCandidateDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &dispatchCmd);

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    gpuCandidateDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    gpuMaskedCandidateDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            }

            if (device.IsMeshShaderSupported()) {
                VkDispatchIndirectCommand taskDispatchCmd{0, 1, 1};
                vkCmdUpdateBuffer(cmd, taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &taskDispatchCmd);

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            } else {
                VkDrawIndirectCommand initialCmd{0, 1, 0, 0};
                vkCmdUpdateBuffer(cmd, singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDrawIndirectCommand), &initialCmd);
                vkCmdUpdateBuffer(cmd, maskedSingleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDrawIndirectCommand), &initialCmd);

                VkDispatchIndirectCommand triDispatchCmd{0, 1, 1};
                vkCmdUpdateBuffer(cmd, triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &triDispatchCmd);
                vkCmdUpdateBuffer(cmd, maskedTriangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &triDispatchCmd);

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    maskedSingleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    maskedTriangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            }

            VkUtils::pipelineBarrier(cmd, 0, 0, 0, 0, {}, transferBarriers);

            if (!CVarFreezeCulling.Get()) {
                activeCullViewProj = frameInfo.cullViewProj;
                activeCullCameraPos = frameInfo.cullCameraPos;
                activeCullView = frameInfo.cullView;
            }

            glm::mat4 proj = frameInfo.camera->getProjection();
            VkExtent2D hizExt = { std::bit_ceil(frameInfo.extent.width), std::bit_ceil(frameInfo.extent.height) };
            uint32_t totalMips = std::bit_width(std::max(hizExt.width, hizExt.height));

            ComputePushConstants compPc {};
            compPc.view              = activeCullView;
            compPc.projParams        = glm::vec4(proj[0][0], proj[1][1], proj[2][2], proj[3][2]);
            compPc.hizParams         = glm::vec4((frameInfo.extent.width * 0.5f) / hizExt.width,
                                                 (frameInfo.extent.height * 0.5f) / hizExt.height,
                                                 static_cast<float>(totalMips),
                                                 frameInfo.camera->getNearClip());
            compPc.screenParams      = glm::vec2(static_cast<float>(frameInfo.extent.width), static_cast<float>(frameInfo.extent.height));
            compPc.cullFlags         = 0;
            if (frameInfo.cullEnabled) compPc.cullFlags |= 1u;
            if (frameInfo.cullEnabled) compPc.cullFlags |= 2u;
            if (frameInfo.cullEnabled && !frameInfo.firstFrame) compPc.cullFlags |= 4u;
            compPc.objectCount       = megaBuffer.getMeshletCount();
            compPc.actualObjectCount = totalObjects;
            compPc.objectCapacity    = Constants::MAX_SCENE_OBJECTS;
            compPc.clipPlaneCount    = 6;
            compPc.phase             = phase;

            auto runCullPipelinePass = [&](VkDescriptorSet cullSet, uint32_t alphaModeTag) {
                compPc.targetAlphaMode = alphaModeTag;

                VkDescriptorSet sets[] = {
                    resourceHeap.getDescriptorSet(currentFrame),
                    cullSet
                };
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 2, sets, 0, nullptr);
                vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPc);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, objectCullPipeline);
                if (phase == 0) {
                    uint32_t objectGroupCount = (compPc.actualObjectCount + Constants::CULL_WORKGROUP_SIZE - 1) / Constants::CULL_WORKGROUP_SIZE;
                    vkCmdDispatch(cmd, objectGroupCount, 1, 1);
                } else if (parentCullPass != nullptr) {
                    VkBuffer candCmdBuf = (alphaModeTag == 0)
                        ? parentCullPass->getCandidateDispatchCommandBuffer(currentFrame)
                        : parentCullPass->getMaskedCandidateDispatchCommandBuffer(currentFrame);
                    vkCmdDispatchIndirect(cmd, candCmdBuf, 0);
                }

                VkMemoryBarrier2 objCullBarrier{};
                objCullBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                objCullBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                objCullBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                objCullBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                objCullBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

                VkDependencyInfo objCullDep{};
                objCullDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                objCullDep.memoryBarrierCount = 1;
                objCullDep.pMemoryBarriers = &objCullBarrier;
                vkCmdPipelineBarrier2(cmd, &objCullDep);

                if (device.IsMeshShaderSupported() && alphaModeTag == 0) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, taskSubmitPipeline);
                    vkCmdDispatchIndirect(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0);

                    VkMemoryBarrier2 taskSubmitBarrier{};
                    taskSubmitBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                    taskSubmitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    taskSubmitBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                    taskSubmitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
                    taskSubmitBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

                    VkDependencyInfo taskSubmitDep{};
                    taskSubmitDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    taskSubmitDep.memoryBarrierCount = 1;
                    taskSubmitDep.pMemoryBarriers = &taskSubmitBarrier;
                    vkCmdPipelineBarrier2(cmd, &taskSubmitDep);
                } else {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, meshletCullPipeline);
                    vkCmdDispatchIndirect(cmd, (alphaModeTag == 0 ? gpuDispatchCommandBuffers[currentFrame] : gpuMaskedDispatchCommandBuffers[currentFrame])->getBuffer(), 0);

                    VkMemoryBarrier2 meshletCullBarrier{};
                    meshletCullBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                    meshletCullBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    meshletCullBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                    meshletCullBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    meshletCullBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

                    VkDependencyInfo meshletCullDep{};
                    meshletCullDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    meshletCullDep.memoryBarrierCount = 1;
                    meshletCullDep.pMemoryBarriers = &meshletCullBarrier;
                    vkCmdPipelineBarrier2(cmd, &meshletCullDep);

                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, triangleCullPipeline);
                    vkCmdDispatchIndirect(cmd, (alphaModeTag == 0 ? triangleDispatchCommandBuffers[currentFrame] : maskedTriangleDispatchCommandBuffers[currentFrame])->getBuffer(), 0);

                    VkMemoryBarrier2 triangleCullBarrier{};
                    triangleCullBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                    triangleCullBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    triangleCullBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                    triangleCullBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                    triangleCullBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

                    VkDependencyInfo triangleCullDep{};
                    triangleCullDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    triangleCullDep.memoryBarrierCount = 1;
                    triangleCullDep.pMemoryBarriers = &triangleCullBarrier;
                    vkCmdPipelineBarrier2(cmd, &triangleCullDep);
                }
            };

            runCullPipelinePass(objectDescriptorSets[currentFrame], 0);
            runCullPipelinePass(maskedObjectDescriptorSets[currentFrame], 1);
        }
    }

    void CullPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        uint32_t currentFrame = frameInfo.frameIndex;
        VkImageView hizView = graph.getImageView("HiZImage");
        if (hizView != VK_NULL_HANDLE) {
            VkDescriptorImageInfo hizInfo {};
            hizInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            hizInfo.imageView = hizView;
            hizInfo.sampler = hizSampler;

            VkWriteDescriptorSet writes[2] {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = objectDescriptorSets[currentFrame];
            writes[0].dstBinding = 8;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &hizInfo;

            writes[1] = writes[0];
            writes[1].dstSet = maskedObjectDescriptorSets[currentFrame];

            vkUpdateDescriptorSets(device.GetHandle(), 2, writes, 0, nullptr);
        }
    }
}