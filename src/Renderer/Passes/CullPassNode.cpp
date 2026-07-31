#include "Renderer/Passes/CullPassNode.h"
#include "Vulkan/Buffer.h"
#include <array>

#include "Core/EngineConfig.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    CullPassNode::CullPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap):
        device(device), megaBuffer(megaBuffer), renderer(renderer), resourceHeap(resourceHeap)
    {
        objectDescriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        std::array<VkDescriptorSetLayoutBinding, 4> ssboBindings {};
        ssboBindings[0].binding = 0;
        ssboBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[0].descriptorCount = 1;
        ssboBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        ssboBindings[1].binding = 1;
        ssboBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[1].descriptorCount = 1;
        ssboBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        ssboBindings[2].binding = 2;
        ssboBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[2].descriptorCount = 1;
        ssboBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        ssboBindings[3].binding = 3;
        ssboBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[3].descriptorCount = 1;
        ssboBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout);

        std::array<VkDescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT * 4;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &objectDescriptorPool);

        gpuDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuVisibleObjectBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        compactedIndexBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        singleIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuDrawCountBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT); // keep temporarily

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

            compactedIndexBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         Config::MAX_SCENE_OBJECTS * Config::MAX_TRIANGLES * 3,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            singleIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndirectCommand),
                                         1,
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            gpuDrawCountBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         1,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            VkDescriptorSetAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]);

            VkDescriptorBufferInfo dispatchInfo = gpuDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleObjInfo = gpuVisibleObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo singleIndirectInfo = singleIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo = compactedIndexBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);            
            
            std::array<VkWriteDescriptorSet, 4> descriptorWrites {};
            
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

            vkUpdateDescriptorSets(device.getDevice(),
                                   static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(),
                                   0,
                                   nullptr);
        }

        createPipeline();
    }

    CullPassNode::~CullPassNode()
    {
        if (objectDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.getDevice(), objectDescriptorPool, nullptr);
        if (objectSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.getDevice(), objectSetLayout, nullptr);
        if (objectCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), objectCullPipeline, nullptr);
        if (meshletCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), meshletCullPipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
    }

    void CullPassNode::createPipeline()
    {
        auto objCode = ShaderUtils::readFile("shaders/object_cull.comp.spv");
        VkShaderModule objModule = ShaderUtils::createShaderModule(device.getDevice(), objCode);

        auto meshletCode = ShaderUtils::readFile("shaders/meshlet_cull.comp.spv");
        VkShaderModule meshletModule = ShaderUtils::createShaderModule(device.getDevice(), meshletCode);

        VkPipelineShaderStageCreateInfo objStageInfo {};
        objStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        objStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        objStageInfo.module = objModule;
        objStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo meshletStageInfo {};
        meshletStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        meshletStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        meshletStageInfo.module = meshletModule;
        meshletStageInfo.pName = "main";

        VkSpecializationMapEntry specializationMapEntry{};
        specializationMapEntry.constantID = 0;
        specializationMapEntry.offset = 0;
        specializationMapEntry.size = sizeof(uint32_t);

        uint32_t workgroupSize = Config::CULL_WORKGROUP_SIZE;

        VkSpecializationInfo specializationInfo{};
        specializationInfo.mapEntryCount = 1;
        specializationInfo.pMapEntries = &specializationMapEntry;
        specializationInfo.dataSize = sizeof(workgroupSize);
        specializationInfo.pData = &workgroupSize;

        objStageInfo.pSpecializationInfo = &specializationInfo;
        meshletStageInfo.pSpecializationInfo = &specializationInfo;

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

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &computePipelineLayout) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create compute pipeline layout");
        }

        VkComputePipelineCreateInfo objPipelineInfo {};
        objPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        objPipelineInfo.layout = computePipelineLayout;
        objPipelineInfo.stage = objStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &objPipelineInfo, nullptr, &objectCullPipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create object cull compute pipeline");
        }

        VkComputePipelineCreateInfo meshletPipelineInfo {};
        meshletPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        meshletPipelineInfo.layout = computePipelineLayout;
        meshletPipelineInfo.stage = meshletStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &meshletPipelineInfo, nullptr, &meshletCullPipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create meshlet cull compute pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), objModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshletModule, nullptr);
    }

    void CullPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        renderGraph.writeBuffer(
            "CullCompactedIndirectCommands", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeBuffer("CullDrawCount", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
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

            for (const auto &obj: *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0)
                    continue;
                // VisBuffers skip transparency completely (render forward/later)
                if (obj.alphaMode == AlphaMode::Blend)
                    continue;

                opaqueDraws.push_back(&obj);
            }

            uint32_t instanceIndex = 0;
            for (const auto *obj: opaqueDraws) {
                VkDrawIndexedIndirectCommand cmdCommand {};
                cmdCommand.indexCount = obj->subMesh.indexCount;
                cmdCommand.instanceCount = 1;
                cmdCommand.firstIndex = obj->subMesh.firstIndex;
                cmdCommand.vertexOffset = obj->subMesh.vertexOffset;
                cmdCommand.firstInstance = instanceIndex++;
                indirectCommandsArray.push_back(cmdCommand);
            }

            framesToUpdate = Config::MAX_FRAMES_IN_FLIGHT;
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
            framesToUpdate--;
        }

        if (!indirectCommandsArray.empty()) {
            vkCmdFillBuffer(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(VkDispatchIndirectCommand), 0);
            vkCmdFillBuffer(cmd, singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, sizeof(uint32_t), 0);

            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &barrier,
                0, nullptr,
                0, nullptr
            );

            VkDescriptorSet sets[] = {resourceHeap.getDescriptorSet(currentFrame), objectDescriptorSets[currentFrame]};
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    computePipelineLayout,
                                    0,
                                    2,
                                    sets,
                                    0,
                                    nullptr);

            ComputePushConstants compPc {};
            compPc.viewProj = viewProjection;
            compPc.cameraPos = frameInfo.camera->getPosition();

            glm::mat4 tvp = glm::transpose(viewProjection);
            compPc.frustumPlanes[0] = tvp[3] + tvp[0]; // Left
            compPc.frustumPlanes[1] = tvp[3] - tvp[0]; // Right
            compPc.frustumPlanes[2] = tvp[3] + tvp[1]; // Bottom
            compPc.frustumPlanes[3] = tvp[3] - tvp[1]; // Top
            compPc.frustumPlanes[4] = tvp[2]; // Near
            compPc.frustumPlanes[5] = tvp[3] - tvp[2]; // Far

            for (int i = 0; i < 6; i++) {
                float len = glm::length(glm::vec3(compPc.frustumPlanes[i]));
                compPc.frustumPlanes[i] /= len;
            }

            compPc.objectCount = megaBuffer.getMeshletCount();
            compPc.actualObjectCount = static_cast<uint32_t>(indirectCommandsArray.size());
            compPc.cascadeIndex = 0;
            compPc.objectCapacity = Config::MAX_SCENE_OBJECTS;
            compPc.clipPlaneCount = 6;
            vkCmdPushConstants(
                cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPc);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, objectCullPipeline);
            uint32_t objectGroupCount = (compPc.actualObjectCount + Config::CULL_WORKGROUP_SIZE - 1) / Config::CULL_WORKGROUP_SIZE;
            vkCmdDispatch(cmd, objectGroupCount, 1, 1);

            VkMemoryBarrier computeBarrier{};
            computeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            computeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                0,
                1, &computeBarrier,
                0, nullptr,
                0, nullptr
            );

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, meshletCullPipeline);
            vkCmdDispatchIndirect(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0);
        }
    }

    void CullPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }
} // namespace Engine