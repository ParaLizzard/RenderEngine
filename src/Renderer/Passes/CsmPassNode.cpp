#include "CsmPassNode.h"

#include <iostream>

#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "System/Input/InputManager.h"
#include "Vulkan/ResourceHeap.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    CsmPassNode::CsmPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, CullPassNode &cullPass)
        : RenderPassNode("CSM Pass"), device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap), cullPass(cullPass)
    {
        objectDescriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        if (device.isMeshShaderSupported()) {
            stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
            pfn_vkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetInstanceProcAddr(device.getInstance(), "vkCmdDrawMeshTasksIndirectEXT");
        }

        std::array<VkDescriptorSetLayoutBinding, 11> ssboBindings {};
        for (uint32_t b = 0; b < 11; b++) {
            ssboBindings[b].binding = b;
            ssboBindings[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ssboBindings[b].descriptorCount = 1;
            ssboBindings[b].stageFlags = stageFlags;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout);

        std::array<VkDescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT * 11;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &objectDescriptorPool);

        gpuDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuVisibleObjectBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        singleIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        compactedIndexBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        visibleMeshletBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        triangleDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        taskWorkgroupBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        taskDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        maskedTaskWorkgroupBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        maskedTaskDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        cascadeDataBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
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
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            singleIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            compactedIndexBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Config::MAX_SCENE_OBJECTS * Config::MAX_TRIANGLES * 3,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            visibleMeshletBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Config::MAX_SCENE_OBJECTS * 100,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            triangleDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            taskWorkgroupBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Config::MAX_SCENE_OBJECTS * 10 * SHADOW_MAP_CASCADES,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            taskDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand) * SHADOW_MAP_CASCADES,
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            maskedTaskWorkgroupBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Config::MAX_SCENE_OBJECTS * 10 * SHADOW_MAP_CASCADES,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            maskedTaskDispatchCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDispatchIndirectCommand) * SHADOW_MAP_CASCADES,
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            cascadeDataBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(CascadeGpuData),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            VkDescriptorSetAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]);
        }
        createPipelineLayout();
        createPipeline();
        if (device.isMeshShaderSupported()) {
            createMeshPipeline();
            createMaskedMeshPipeline();
        }
    }

    CsmPassNode::~CsmPassNode()
    {
        if (objectDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.getDevice(), objectDescriptorPool, nullptr);
        if (objectSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.getDevice(), objectSetLayout, nullptr);

        if (objectCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), objectCullPipeline, nullptr);
        if (taskSubmitPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), taskSubmitPipeline, nullptr);
        if (meshletCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), meshletCullPipeline, nullptr);
        if (triangleCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), triangleCullPipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);

        if (meshPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), meshPipeline, nullptr);
        if (maskedMeshPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), maskedMeshPipeline, nullptr);
        if (meshPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), meshPipelineLayout, nullptr);
        if (maskedPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), maskedPipeline, nullptr);
        if (maskedPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), maskedPipelineLayout, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void CsmPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D mapExtent = {SHADOW_ATLAS_WIDTH, SHADOW_ATLAS_HEIGHT};
        renderGraph.createTransientImage(
            "CsmImage",
            Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT,
            mapExtent,
            1,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        );

        renderGraph.writeImage(
            "CsmImage",
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        );
    }

    void CsmPassNode::updateDescriptors()
    {
        for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo dispatchInfo = gpuDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleObjInfo = gpuVisibleObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo singleIndirectInfo = singleIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo = compactedIndexBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleMeshletsInfo = visibleMeshletBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo triangleDispatchInfo = triangleDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskWorkgroupInfo = taskWorkgroupBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskDispatchInfo = taskDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo cascadeInfo = cascadeDataBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedTaskWorkgroupInfo = maskedTaskWorkgroupBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo maskedTaskDispatchInfo = maskedTaskDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 11> descriptorWrites {};

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
            descriptorWrites[8].dstBinding = 8;
            descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[8].descriptorCount = 1;
            descriptorWrites[8].pBufferInfo = &cascadeInfo;

            descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[9].dstSet = objectDescriptorSets[i];
            descriptorWrites[9].dstBinding = 9;
            descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[9].descriptorCount = 1;
            descriptorWrites[9].pBufferInfo = &maskedTaskWorkgroupInfo;

            descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[10].dstSet = objectDescriptorSets[i];
            descriptorWrites[10].dstBinding = 10;
            descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[10].descriptorCount = 1;
            descriptorWrites[10].pBufferInfo = &maskedTaskDispatchInfo;

            vkUpdateDescriptorSets(device.getDevice(),
                                   static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(),
                                   0,
                                   nullptr);
        }
        descriptorsUpdated = true;
    }

    void CsmPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        if (!descriptorsUpdated) {
            updateDescriptors();
        }

        uint32_t currentFrame = renderer.getFrameIndex();
        uint32_t totalObjects = static_cast<uint32_t>(frameInfo.gameObjects->size());

        if (totalObjects > 0) {
            CascadeGpuData cascadeData {};
            for (uint32_t c = 0; c < SHADOW_MAP_CASCADES; c++) {
                cascadeData.viewProj[c] = cascadeViewProjs[c];

                glm::mat4 tvp = glm::transpose(cascadeViewProjs[c]);
                glm::vec4 planes[6] = {
                    tvp[3] + tvp[0], // Left
                    tvp[3] - tvp[0], // Right
                    tvp[3] + tvp[1], // Bottom
                    tvp[3] - tvp[1], // Top
                    tvp[2],          // Near
                    tvp[3] - tvp[2]  // Far
                };

                for (int p = 0; p < 6; p++) {
                    float invLen = 1.0f / glm::length(glm::vec3(planes[p]));
                    cascadeData.frustumPlanes[c * 6 + p] = planes[p] * invLen;
                }
            }
            const glm::mat4 &v = frameInfo.camera->getView();
            glm::vec3 camFwd = glm::vec3(v[0][2], v[1][2], v[2][2]);
            glm::vec3 camPos = frameInfo.camera->getPosition();
            cascadeData.cameraForward = glm::vec4(camFwd, glm::dot(camFwd, camPos));
            cascadeDataBuffers[currentFrame]->writeToBuffer(&cascadeData, sizeof(CascadeGpuData), 0);
            cascadeDataBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

            VkDispatchIndirectCommand dispatchCmd{0, 1, 1};
            vkCmdUpdateBuffer(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &dispatchCmd);

            std::vector<VkBufferMemoryBarrier2> transferBarriers;
            transferBarriers.push_back(VkUtils::bufferBarrier(
                gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));

            if (device.isMeshShaderSupported()) {
                VkDispatchIndirectCommand taskDispatchCmd[SHADOW_MAP_CASCADES] = {
                    {0, 1, 1},
                    {0, 1, 1},
                    {0, 1, 1}
                };
                vkCmdUpdateBuffer(cmd, taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(taskDispatchCmd), taskDispatchCmd);
                vkCmdUpdateBuffer(cmd, maskedTaskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(taskDispatchCmd), taskDispatchCmd);
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    maskedTaskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            } else {
                VkDrawIndirectCommand initialCmd{0, 1, 0, 0};
                vkCmdUpdateBuffer(cmd, singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDrawIndirectCommand), &initialCmd);

                VkDispatchIndirectCommand triDispatchCmd{0, 1, 1};
                vkCmdUpdateBuffer(cmd, triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), &triDispatchCmd);

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            }

            transferBarriers.push_back(VkUtils::bufferBarrier(
                cascadeDataBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));

            VkUtils::pipelineBarrier(cmd, 0, 0, 0, 0, {}, transferBarriers);

            VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
            VkDescriptorSet computeSets[] = {bindlessSet, objectDescriptorSets[currentFrame]};

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 2, computeSets, 0, nullptr);

            CsmCullPushConstants compPc {};
            compPc.objectCount = megaBuffer.getMeshletCount();
            compPc.actualObjectCount = totalObjects;
            compPc.cullFlags = frameInfo.cullEnabled ? 1 : 0;
            compPc.maxTaskWgsPerCascade = Config::MAX_SCENE_OBJECTS * 10;
            vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CsmCullPushConstants), &compPc);

            if (frameInfo.renderGraph) frameInfo.renderGraph->pushProfileMarker(cmd, "CSM Compute Pre-pass");

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, objectCullPipeline);
            uint32_t objectGroupCount = (totalObjects + Config::CULL_WORKGROUP_SIZE - 1) / Config::CULL_WORKGROUP_SIZE;
            vkCmdDispatch(cmd, objectGroupCount, 1, 1);

            VkMemoryBarrier2 objCullBarrier{};
            objCullBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            objCullBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            objCullBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            objCullBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            objCullBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

            VkDependencyInfo objCullDep{};
            objCullDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            objCullDep.memoryBarrierCount = 1;
            objCullDep.pMemoryBarriers = &objCullBarrier;
            vkCmdPipelineBarrier2(cmd, &objCullDep);

            if (!device.isMeshShaderSupported()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, meshletCullPipeline);
                vkCmdDispatchIndirect(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0);

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
                vkCmdDispatchIndirect(cmd, triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0);

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
            if (frameInfo.renderGraph) frameInfo.renderGraph->popProfileMarker(cmd);
        }

        VkRenderingAttachmentInfo depthAttachment {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = frameInfo.renderGraph->getImageView("CsmImage");
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo renderingInfo {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = {SHADOW_ATLAS_WIDTH, SHADOW_ATLAS_HEIGHT};
        renderingInfo.viewMask = 0;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pDepthAttachment = &depthAttachment;

        if (frameInfo.renderGraph) frameInfo.renderGraph->pushProfileMarker(cmd, "CSM Render Pass");

        vkCmdBeginRendering(cmd, &renderingInfo);

        const VkViewport viewports[SHADOW_MAP_CASCADES] = {
            {0.0f, 0.0f, static_cast<float>(SHADOW_CASCADE0_SIZE), static_cast<float>(SHADOW_CASCADE0_SIZE), 0.0f, 1.0f},
            {0.0f, 2048.0f, static_cast<float>(SHADOW_CASCADE1_SIZE), static_cast<float>(SHADOW_CASCADE1_SIZE), 0.0f, 1.0f},
            {1024.0f, 2048.0f, static_cast<float>(SHADOW_CASCADE2_SIZE), static_cast<float>(SHADOW_CASCADE2_SIZE), 0.0f, 1.0f}
        };

        const VkRect2D scissors[SHADOW_MAP_CASCADES] = {
            {{0, 0}, {SHADOW_CASCADE0_SIZE, SHADOW_CASCADE0_SIZE}},
            {{0, 2048}, {SHADOW_CASCADE1_SIZE, SHADOW_CASCADE1_SIZE}},
            {{1024, 2048}, {SHADOW_CASCADE2_SIZE, SHADOW_CASCADE2_SIZE}}
        };

        for (uint32_t c = 0; c < SHADOW_MAP_CASCADES; ++c) {
            vkCmdSetViewport(cmd, 0, 1, &viewports[c]);
            vkCmdSetScissor(cmd, 0, 1, &scissors[c]);

            VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
            VkDescriptorSet sets[] = {bindlessSet, objectDescriptorSets[currentFrame]};

            if (totalObjects > 0) {
                CsmMeshPushConstants meshPc { c, Config::MAX_SCENE_OBJECTS * 10 };
                if (device.isMeshShaderSupported()) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout, 0, 2, sets, 0, nullptr);
                    vkCmdPushConstants(cmd, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(CsmMeshPushConstants), &meshPc);
                    VkDeviceSize indirectOffset = c * sizeof(VkDispatchIndirectCommand);
                    pfn_vkCmdDrawMeshTasksIndirectEXT(cmd, taskDispatchCommandBuffers[currentFrame]->getBuffer(), indirectOffset, 1, sizeof(VkDispatchIndirectCommand));
                } else {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);
                    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(CsmMeshPushConstants), &meshPc);
                    vkCmdDrawIndirect(cmd, singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, 1, sizeof(VkDrawIndirectCommand));
                }
            }
        }

        vkCmdEndRendering(cmd);
        if (frameInfo.renderGraph) frameInfo.renderGraph->popProfileMarker(cmd);
    }

    void CsmPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }

    void CsmPassNode::createPipelineLayout()
    {
        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();
        VkDescriptorSetLayout layouts[] = {bindlessLayout, objectSetLayout};

        VkPushConstantRange pushConstantRangeCascade {};
        pushConstantRangeCascade.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
        pushConstantRangeCascade.offset = 0;
        pushConstantRangeCascade.size = sizeof(CsmMeshPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRangeCascade;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);
        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &meshPipelineLayout);
        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &maskedPipelineLayout);

        VkPushConstantRange pushConstantRangeCompute {};
        pushConstantRangeCompute.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRangeCompute.offset = 0;
        pushConstantRangeCompute.size = sizeof(CsmCullPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutComputeInfo {};
        pipelineLayoutComputeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutComputeInfo.setLayoutCount = 2;
        pipelineLayoutComputeInfo.pSetLayouts = layouts;
        pipelineLayoutComputeInfo.pushConstantRangeCount = 1;
        pipelineLayoutComputeInfo.pPushConstantRanges = &pushConstantRangeCompute;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutComputeInfo, nullptr, &computePipelineLayout);
    }

    void CsmPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/shadow.vert.spv");
        auto objCode = ShaderUtils::readFile("shaders/csm_cull.comp.spv");
        auto taskSubmitCode = ShaderUtils::readFile("shaders/task_submit.comp.spv");
        auto meshletCode = ShaderUtils::readFile("shaders/csm_meshlet_cull.comp.spv");
        auto triangleCode = ShaderUtils::readFile("shaders/csm_triangle_cull.comp.spv");

        VkShaderModule vertShaderModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule objModule = ShaderUtils::createShaderModule(device.getDevice(), objCode);
        VkShaderModule taskSubmitModule = ShaderUtils::createShaderModule(device.getDevice(), taskSubmitCode);
        VkShaderModule meshletModule = ShaderUtils::createShaderModule(device.getDevice(), meshletCode);
        VkShaderModule triangleModule = ShaderUtils::createShaderModule(device.getDevice(), triangleCode);

        VkPipelineShaderStageCreateInfo shaderStages[1] {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 3.0f;
        rasterizer.depthClampEnable = VK_TRUE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 0;
        renderingCreateInfo.pColorAttachmentFormats = nullptr;
        renderingCreateInfo.depthAttachmentFormat = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
        renderingCreateInfo.viewMask = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingCreateInfo;
        pipelineInfo.stageCount = 1;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;

        vkCreateGraphicsPipelines(
            device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &pipeline);

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);

        auto createCompPipeline = [&](VkShaderModule module, VkPipeline &outPipeline) {
            VkPipelineShaderStageCreateInfo stageInfo {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            stageInfo.module = module;
            stageInfo.pName = "main";

            VkComputePipelineCreateInfo cpInfo {};
            cpInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            cpInfo.layout = computePipelineLayout;
            cpInfo.stage = stageInfo;

            vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &cpInfo, nullptr, &outPipeline);
        };

        createCompPipeline(objModule, objectCullPipeline);
        createCompPipeline(taskSubmitModule, taskSubmitPipeline);
        createCompPipeline(meshletModule, meshletCullPipeline);
        createCompPipeline(triangleModule, triangleCullPipeline);

        vkDestroyShaderModule(device.getDevice(), objModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), taskSubmitModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshletModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), triangleModule, nullptr);

        createMaskedPipeline();
    }

    void CsmPassNode::createMeshPipeline()
    {
        auto taskCode = ShaderUtils::readFile("shaders/csm_meshlet.task.spv");
        auto meshCode = ShaderUtils::readFile("shaders/csm_triangle.mesh.spv");

        VkShaderModule taskShaderModule = ShaderUtils::createShaderModule(device.getDevice(), taskCode);
        VkShaderModule meshShaderModule = ShaderUtils::createShaderModule(device.getDevice(), meshCode);

        VkPipelineShaderStageCreateInfo shaderStages[2] {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
        shaderStages[0].module = taskShaderModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        shaderStages[1].module = meshShaderModule;
        shaderStages[1].pName = "main";

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 3.0f;
        rasterizer.depthClampEnable = VK_TRUE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 0;
        renderingCreateInfo.pColorAttachmentFormats = nullptr;
        renderingCreateInfo.depthAttachmentFormat = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
        renderingCreateInfo.viewMask = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingCreateInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = meshPipelineLayout;

        if (vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &meshPipeline) != VK_SUCCESS) {
            throw std::runtime_error("CsmPassNode: failed to create mesh shadow pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), taskShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshShaderModule, nullptr);
    }

    void CsmPassNode::createMaskedMeshPipeline()
    {
        auto taskCode = ShaderUtils::readFile("shaders/csm_meshlet_masked.task.spv");
        auto meshCode = ShaderUtils::readFile("shaders/csm_triangle_masked.mesh.spv");
        auto fragCode = ShaderUtils::readFile("shaders/shadow_masked.frag.spv");

        VkShaderModule taskShaderModule = ShaderUtils::createShaderModule(device.getDevice(), taskCode);
        VkShaderModule meshShaderModule = ShaderUtils::createShaderModule(device.getDevice(), meshCode);
        VkShaderModule fragShaderModule = ShaderUtils::createShaderModule(device.getDevice(), fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[3] {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
        shaderStages[0].module = taskShaderModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
        shaderStages[1].module = meshShaderModule;
        shaderStages[1].pName = "main";

        shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[2].module = fragShaderModule;
        shaderStages[2].pName = "main";

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 3.0f;
        rasterizer.depthClampEnable = VK_TRUE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 0;
        renderingCreateInfo.pColorAttachmentFormats = nullptr;
        renderingCreateInfo.depthAttachmentFormat = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
        renderingCreateInfo.viewMask = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingCreateInfo;
        pipelineInfo.stageCount = 3;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = meshPipelineLayout; // Re-use the same layout

        if (vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &maskedMeshPipeline) != VK_SUCCESS) {
            throw std::runtime_error("CsmPassNode: failed to create masked mesh shadow pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), taskShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void CsmPassNode::createMaskedPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/shadow_masked.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/shadow_masked.frag.spv");

        VkShaderModule vertShaderModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule fragShaderModule = ShaderUtils::createShaderModule(device.getDevice(), fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2] {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 3.0f;
        rasterizer.depthClampEnable = VK_TRUE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 0;
        renderingCreateInfo.pColorAttachmentFormats = nullptr;
        renderingCreateInfo.depthAttachmentFormat = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
        renderingCreateInfo.viewMask = 0;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingCreateInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = maskedPipelineLayout;

        if (vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &maskedPipeline) != VK_SUCCESS) {
            throw std::runtime_error("CsmPassNode: failed to create masked shadow pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void CsmPassNode::updateCascades(SceneUbo &sceneUbo, FrameInfo &frameInfo)
    {
        float nearClip = frameInfo.camera->getNearClip();
        const float maxShadowDistance = 90.0f;
        float farClip = std::min(frameInfo.camera->getFarClip(), maxShadowDistance);
        float clipRange = farClip - nearClip;

        float minZ = nearClip;
        float maxZ = nearClip + clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        float cascadeSplitLambda = 0.75f;

        for (uint32_t i = 0; i < SHADOW_MAP_CASCADES; i++) {
            float p = (i+1) / static_cast<float>(SHADOW_MAP_CASCADES);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = cascadeSplitLambda * (log - uniform) + uniform;
            sceneUbo.cascadesSplits[i] = d;
        }

        float lastSplitDist = 0.0f;
        for (uint32_t i = 0; i < SHADOW_MAP_CASCADES; i++) {
            float splitDist = (sceneUbo.cascadesSplits[i] - nearClip) / clipRange;

            glm::vec3 frustumCorners[8] = {
                glm::vec3(-1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),
            };

            glm::mat4 invCam = glm::inverse(frameInfo.camera->getProjection() * frameInfo.camera->getView());
            for (uint32_t j = 0; j < 8; j++) {
                glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
                frustumCorners[j] = invCorner / invCorner.w;
            }

            for (uint32_t j = 0; j < 4; j++) {
                glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
                frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
            }

            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (uint32_t j = 0; j < 8; j++) {
                frustumCenter += frustumCorners[j];
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (uint32_t j = 0; j < 8; j++) {
                float distance = glm::length(frustumCorners[j] - frustumCenter);
                radius = glm::max(radius, distance);
            }

            radius *= 1.05f;
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            glm::vec3 lightDir = glm::normalize(glm::vec3(sceneUbo.directionalLight));
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(lightDir.y) > 0.999f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            float zMultiplier = 100.0f;

            float nearPlane = 0.0f;
            float farPlane  = zMultiplier + radius * 2.0f;

            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter + lightDir * zMultiplier, frustumCenter, up);

            glm::vec3 sceneMin(1e30f);
            glm::vec3 sceneMax(-1e30f);
            bool hasVisible = false;

            for (const auto& obj : *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0) continue;
                
                glm::vec4 center = obj.currentWorldMatrix * glm::vec4(obj.boundingSphere.x, obj.boundingSphere.y, obj.boundingSphere.z, 1.0f);
                
                float sqX = glm::dot(glm::vec3(obj.currentWorldMatrix[0]), glm::vec3(obj.currentWorldMatrix[0]));
                float sqY = glm::dot(glm::vec3(obj.currentWorldMatrix[1]), glm::vec3(obj.currentWorldMatrix[1]));
                float sqZ = glm::dot(glm::vec3(obj.currentWorldMatrix[2]), glm::vec3(obj.currentWorldMatrix[2]));
                float maxScale = std::sqrt(std::max({sqX, sqY, sqZ}));
                float worldRadius = obj.boundingSphere.w * maxScale;
                
                glm::vec4 lsCenter = lightViewMatrix * center;

                if (lsCenter.x + worldRadius < minExtents.x || lsCenter.x - worldRadius > maxExtents.x ||
                    lsCenter.y + worldRadius < minExtents.y || lsCenter.y - worldRadius > maxExtents.y) {
                    continue;
                }

                sceneMin.x = std::min(sceneMin.x, lsCenter.x - worldRadius);
                sceneMax.x = std::max(sceneMax.x, lsCenter.x + worldRadius);
                sceneMin.y = std::min(sceneMin.y, lsCenter.y - worldRadius);
                sceneMax.y = std::max(sceneMax.y, lsCenter.y + worldRadius);
                sceneMin.z = std::min(sceneMin.z, lsCenter.z - worldRadius);
                sceneMax.z = std::max(sceneMax.z, lsCenter.z + worldRadius);
                hasVisible = true;
            }

            if (hasVisible) {
                nearPlane = -sceneMax.z - 20.0f;
                farPlane  = -sceneMin.z + 20.0f;


                float step = 2.0f;
                minExtents.x = std::floor(minExtents.x / step) * step;
                maxExtents.x = std::ceil(maxExtents.x / step) * step;
                minExtents.y = std::floor(minExtents.y / step) * step;
                maxExtents.y = std::ceil(maxExtents.y / step) * step;
            }

            glm::mat4 lightOrthoMatrix = glm::orthoZO(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, nearPlane, farPlane);

            lightOrthoMatrix[1][1] *= -1.0f;

            glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
            glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            const float cascadeSizes[SHADOW_MAP_CASCADES] = {
                static_cast<float>(SHADOW_CASCADE0_SIZE),
                static_cast<float>(SHADOW_CASCADE1_SIZE),
                static_cast<float>(SHADOW_CASCADE2_SIZE)
            };
            float shadowMapSize = cascadeSizes[i];
            glm::vec2 shadowOffset = glm::vec2(shadowOrigin.x, shadowOrigin.y) * (shadowMapSize / 2.0f);
            glm::vec2 roundedOffset = glm::round(shadowOffset);
            glm::vec2 subTexelOffset = roundedOffset - shadowOffset;
            subTexelOffset /= (shadowMapSize / 2.0f);
            lightOrthoMatrix[3][0] += subTexelOffset.x;
            lightOrthoMatrix[3][1] += subTexelOffset.y;

            cascadeViewProjs[i] = lightOrthoMatrix * lightViewMatrix;
            sceneUbo.lightViewProj[i] = cascadeViewProjs[i];

            lastSplitDist = splitDist;
        }
    }
} // namespace Engine