#include "Renderer/Passes/CullPassNode.h"
#include "Vulkan/Buffer.h"
#include <array>
#include <vector>

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

        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        if (device.isMeshShaderSupported()) {
            stageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
        }

        std::array<VkDescriptorSetLayoutBinding, 8> ssboBindings {};
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

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout);

        std::array<VkDescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT * 8;

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
        gpuDrawCountBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        triangleDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        visibleMeshletBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        taskWorkgroupBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        taskDispatchCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

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
                                         sizeof(uint32_t) * 2, // uvec2
                                         Config::MAX_SCENE_OBJECTS * 100, // Safe estimate for maximum visible meshlets
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            taskWorkgroupBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t) * 2,
                                         Config::MAX_SCENE_OBJECTS * 10,
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
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            VkDescriptorSetAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            if (vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate object descriptor sets in CullPassNode");
            }

            VkDescriptorBufferInfo dispatchInfo = gpuDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleObjInfo = gpuVisibleObjectBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo singleIndirectInfo = singleIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo = compactedIndexBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo visibleMeshletsInfo = visibleMeshletBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo triangleDispatchInfo = triangleDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskWorkgroupInfo = taskWorkgroupBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo taskDispatchInfo = taskDispatchCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 8> descriptorWrites {};

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
        if (taskSubmitPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), taskSubmitPipeline, nullptr);
        if (meshletCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), meshletCullPipeline, nullptr);
        if (triangleCullPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), triangleCullPipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
    }

    void CullPassNode::createPipeline()
    {
        auto objCode = ShaderUtils::readFile("shaders/object_cull.comp.spv");
        VkShaderModule objModule = ShaderUtils::createShaderModule(device.getDevice(), objCode);

        auto taskSubmitCode = ShaderUtils::readFile("shaders/task_submit.comp.spv");
        VkShaderModule taskSubmitModule = ShaderUtils::createShaderModule(device.getDevice(), taskSubmitCode);

        auto meshletCode = ShaderUtils::readFile("shaders/meshlet_cull.comp.spv");
        VkShaderModule meshletModule = ShaderUtils::createShaderModule(device.getDevice(), meshletCode);

        auto triangleCode = ShaderUtils::readFile("shaders/triangle_cull.comp.spv");
        VkShaderModule triangleModule = ShaderUtils::createShaderModule(device.getDevice(), triangleCode);

        VkPipelineShaderStageCreateInfo objStageInfo {};
        objStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        objStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        objStageInfo.module = objModule;
        objStageInfo.pName = "main";

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

        VkPipelineShaderStageCreateInfo triangleStageInfo {};
        triangleStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        triangleStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        triangleStageInfo.module = triangleModule;
        triangleStageInfo.pName = "main";

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

        uint32_t triWorkgroupSize = 128;
        VkSpecializationInfo triSpecializationInfo{};
        triSpecializationInfo.mapEntryCount = 1;
        triSpecializationInfo.pMapEntries = &specializationMapEntry;
        triSpecializationInfo.dataSize = sizeof(triWorkgroupSize);
        triSpecializationInfo.pData = &triWorkgroupSize;
        triangleStageInfo.pSpecializationInfo = &triSpecializationInfo;

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

        VkComputePipelineCreateInfo taskSubmitPipelineInfo {};
        taskSubmitPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        taskSubmitPipelineInfo.layout = computePipelineLayout;
        taskSubmitPipelineInfo.stage = taskSubmitStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &taskSubmitPipelineInfo, nullptr, &taskSubmitPipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create task submit compute pipeline");
        }

        VkComputePipelineCreateInfo meshletPipelineInfo {};
        meshletPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        meshletPipelineInfo.layout = computePipelineLayout;
        meshletPipelineInfo.stage = meshletStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &meshletPipelineInfo, nullptr, &meshletCullPipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create meshlet cull compute pipeline");
        }

        VkComputePipelineCreateInfo trianglePipelineInfo {};
        trianglePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        trianglePipelineInfo.layout = computePipelineLayout;
        trianglePipelineInfo.stage = triangleStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &trianglePipelineInfo, nullptr, &triangleCullPipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("CullPassNode: failed to create triangle cull compute pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), objModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), taskSubmitModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshletModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), triangleModule, nullptr);
    }

    void CullPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        if (!device.isMeshShaderSupported()) {
            renderGraph.writeBuffer("CompactedIndexBuffer",
                                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.writeBuffer("SingleIndirectCommand",
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_WRITE_BIT);
        }
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

            for (size_t i = 0; i < frameInfo.gameObjects->size(); i++) {
                const auto &obj = (*frameInfo.gameObjects)[i];
                if (obj.subMesh.indexCount == 0)
                    continue;
                if (obj.alphaMode == AlphaMode::Blend)
                    continue;

                opaqueDraws.push_back(&obj);

                VkDrawIndexedIndirectCommand cmdCommand {};
                cmdCommand.indexCount = obj.subMesh.indexCount;
                cmdCommand.instanceCount = 1;
                cmdCommand.firstIndex = obj.subMesh.firstIndex;
                cmdCommand.vertexOffset = obj.subMesh.vertexOffset;
                cmdCommand.firstInstance = static_cast<uint32_t>(i);
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
            VkDispatchIndirectCommand dispatchCmd{0, 1, 1};
            vkCmdUpdateBuffer(
                cmd,
                gpuDispatchCommandBuffers[currentFrame]->getBuffer(),
                0,
                sizeof(VkDispatchIndirectCommand),
                &dispatchCmd
            );

            std::vector<VkBufferMemoryBarrier2> transferBarriers;
            transferBarriers.push_back(VkUtils::bufferBarrier(
                gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));

            if (device.isMeshShaderSupported()) {
                VkDispatchIndirectCommand taskDispatchCmd{0, 1, 1};
                vkCmdUpdateBuffer(
                    cmd,
                    taskDispatchCommandBuffers[currentFrame]->getBuffer(),
                    0,
                    sizeof(VkDispatchIndirectCommand),
                    &taskDispatchCmd
                );

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            } else {
                VkDrawIndirectCommand initialCmd{0, 1, 0, 0};
                vkCmdUpdateBuffer(
                    cmd,
                    singleIndirectCommandBuffers[currentFrame]->getBuffer(),
                    0,
                    sizeof(VkDrawIndirectCommand),
                    &initialCmd
                );

                VkDispatchIndirectCommand triDispatchCmd{0, 1, 1};
                vkCmdUpdateBuffer(
                    cmd,
                    triangleDispatchCommandBuffers[currentFrame]->getBuffer(),
                    0,
                    sizeof(VkDispatchIndirectCommand),
                    &triDispatchCmd
                );

                transferBarriers.push_back(VkUtils::bufferBarrier(
                    singleIndirectCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
                transferBarriers.push_back(VkUtils::bufferBarrier(
                    triangleDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT));
            }

            VkUtils::pipelineBarrier(cmd, 0, 0, 0, 0, {}, transferBarriers);

            VkDescriptorSet sets[] = {
                resourceHeap.getDescriptorSet(currentFrame),
                objectDescriptorSets[currentFrame]
            };
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                computePipelineLayout,
                0,
                2,
                sets,
                0,
                nullptr
            );

            ComputePushConstants compPc {};
            compPc.viewProj  = viewProjection;
            compPc.cameraPos = frameInfo.cullCameraPos;

            glm::mat4 tvp = glm::transpose(frameInfo.cullViewProj);
            compPc.frustumPlanes[0] = tvp[3] + tvp[0]; // Left
            compPc.frustumPlanes[1] = tvp[3] - tvp[0]; // Right
            compPc.frustumPlanes[2] = tvp[3] + tvp[1]; // Bottom
            compPc.frustumPlanes[3] = tvp[3] - tvp[1]; // Top
            compPc.frustumPlanes[4] = tvp[2];          // Near
            compPc.frustumPlanes[5] = tvp[3] - tvp[2]; // Far

            for (int i = 0; i < 6; i++) {
                float len = glm::length(glm::vec3(compPc.frustumPlanes[i]));
                compPc.frustumPlanes[i] /= len;
            }

            compPc.cullFlags          = frameInfo.cullEnabled ? 1 : 0;


            compPc.objectCount       = megaBuffer.getMeshletCount();
            compPc.actualObjectCount = static_cast<uint32_t>(frameInfo.gameObjects->size());
            compPc.projM11           = projection[1][1];
            compPc.objectCapacity    = Config::MAX_SCENE_OBJECTS;
            compPc.clipPlaneCount    = 6;

            vkCmdPushConstants(
                cmd,
                computePipelineLayout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(ComputePushConstants),
                &compPc
            );

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, objectCullPipeline);
            uint32_t objectGroupCount = (compPc.actualObjectCount + Config::CULL_WORKGROUP_SIZE - 1) / Config::CULL_WORKGROUP_SIZE;
            vkCmdDispatch(cmd, objectGroupCount, 1, 1);

            std::vector<VkBufferMemoryBarrier2> objectCullBarriers = {
                VkUtils::bufferBarrier(
                    gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT),
                VkUtils::bufferBarrier(
                    gpuVisibleObjectBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            };

            VkUtils::pipelineBarrier(cmd, 0, 0, 0, 0, {}, objectCullBarriers);

            if (device.isMeshShaderSupported()) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, taskSubmitPipeline);
                vkCmdDispatchIndirect(cmd, gpuDispatchCommandBuffers[currentFrame]->getBuffer(), 0);

                std::vector<VkBufferMemoryBarrier2> taskSubmitBarriers = {
                    VkUtils::bufferBarrier(
                        taskWorkgroupBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT, VK_ACCESS_2_SHADER_READ_BIT),
                    VkUtils::bufferBarrier(
                        taskDispatchCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
                };

                VkUtils::pipelineBarrier(cmd, 0, 0, 0, 0, {}, taskSubmitBarriers);
            } else {
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
        }
    }

    void CullPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }
} // namespace Engine