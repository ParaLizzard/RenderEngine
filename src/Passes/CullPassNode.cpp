#include "CullPassNode.h"
#include "Core/Buffer.h"
#include <array>

#include "Core/EngineConfig.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Core/VkUtils.h"

namespace Engine {
    CullPassNode::CullPassNode(Device &device, Renderer &renderer, Model &megaBuffer):
        device(device), megaBuffer(megaBuffer), renderer(renderer)
    {
        objectDescriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        std::array<VkDescriptorSetLayoutBinding, 4> ssboBindings {};
        ssboBindings[0].binding = 0;
        ssboBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[0].descriptorCount = 1;
        ssboBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

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

        cpuObjectSSBOs.resize(Config::MAX_FRAMES_IN_FLIGHT);
        cpuIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuObjectSSBOs.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuCompactedIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuDrawCountBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            cpuObjectSSBOs[i] = std::make_unique<Buffer>(device,
                                                         sizeof(ObjectData),
                                                         Config::MAX_SCENE_OBJECTS,
                                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                         0);
            cpuIndirectCommandBuffers[i] = std::make_unique<Buffer>(device,
                                                                    sizeof(VkDrawIndexedIndirectCommand),
                                                                    Config::MAX_SCENE_OBJECTS,
                                                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                                    VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                                    0);
            gpuObjectSSBOs[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(ObjectData),
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);
            gpuIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuCompactedIndirectCommandBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(VkDrawIndexedIndirectCommand),
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuDrawCountBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         1,
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            VkDescriptorSetAllocateInfo allocInfo {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]);

            VkDescriptorBufferInfo bufferInfo = gpuObjectSSBOs[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo blueprintInfo = gpuIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo countInfo = gpuDrawCountBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo =
                gpuCompactedIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 4> descriptorWrites {};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = objectDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = objectDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &blueprintInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = objectDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &countInfo;

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
        if (computePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), computePipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
    }

    void CullPassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/cull.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo {};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &objectSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &computePipelineLayout);

        VkComputePipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }

    void CullPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        renderGraph.readBuffer("CullObjectData", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
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
            objectDataArray.clear();
            indirectCommandsArray.clear();

            for (const auto &obj: *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0)
                    continue;
                // VisBuffers skip transparency completely (render forward/later)
                if (obj.alphaMode == AlphaMode::Blend)
                    continue;

                opaqueDraws.push_back(&obj);
            }

            for (const auto *obj: opaqueDraws) {
                ObjectData data {};
                data.modelMatrix = obj->currentWorldMatrix;
                data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(obj->currentWorldMatrix))));
                data.boundingSphere = obj->boundingSphere;
                objectDataArray.push_back(data);

                VkDrawIndexedIndirectCommand cmdCommand {};
                cmdCommand.indexCount = obj->subMesh.indexCount;
                cmdCommand.instanceCount = 1;
                cmdCommand.firstIndex = obj->subMesh.firstIndex;
                cmdCommand.vertexOffset = obj->subMesh.vertexOffset;
                cmdCommand.firstInstance = objectDataArray.size() - 1;
                indirectCommandsArray.push_back(cmdCommand);
            }

            framesToUpdate = Config::MAX_FRAMES_IN_FLIGHT;
            sceneDirty = false;
        }

        if (framesToUpdate > 0) {
            if (!objectDataArray.empty()) {
                cpuObjectSSBOs[currentFrame]->writeToBuffer(
                    objectDataArray.data(), objectDataArray.size() * sizeof(ObjectData), 0);
                cpuObjectSSBOs[currentFrame]->flush(VK_WHOLE_SIZE, 0);
                cpuIndirectCommandBuffers[currentFrame]->writeToBuffer(
                    indirectCommandsArray.data(),
                    indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand),
                    0);
                cpuIndirectCommandBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

                VkBufferCopy objCopy {};
                objCopy.size = objectDataArray.size() * sizeof(ObjectData);
                vkCmdCopyBuffer(cmd,
                                cpuObjectSSBOs[currentFrame]->getBuffer(),
                                gpuObjectSSBOs[currentFrame]->getBuffer(),
                                1,
                                &objCopy);

                VkBufferCopy indCopy {};
                indCopy.size = indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand);
                vkCmdCopyBuffer(cmd,
                                cpuIndirectCommandBuffers[currentFrame]->getBuffer(),
                                gpuIndirectCommandBuffers[currentFrame]->getBuffer(),
                                1,
                                &indCopy);

                std::array<VkBufferMemoryBarrier2, 2> copyBarriers {};
                copyBarriers[0] = VkUtils::bufferBarrier(
                    gpuObjectSSBOs[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT);

                copyBarriers[1] = copyBarriers[0];
                copyBarriers[1].buffer = gpuIndirectCommandBuffers[currentFrame]->getBuffer();

                VkDependencyInfo copyDependency {};
                copyDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                copyDependency.bufferMemoryBarrierCount = 2;
                copyDependency.pBufferMemoryBarriers = copyBarriers.data();
                vkCmdPipelineBarrier2(cmd, &copyDependency);
            }
            framesToUpdate--;
        }

        if (!objectDataArray.empty()) {
            vkCmdFillBuffer(cmd, gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, sizeof(uint32_t), 0);

            VkBufferMemoryBarrier2 clearBarrier = VkUtils::bufferBarrier(
                gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDependencyInfo clearDep {};
            clearDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            clearDep.bufferMemoryBarrierCount = 1;
            clearDep.pBufferMemoryBarriers = &clearBarrier;
            vkCmdPipelineBarrier2(cmd, &clearDep);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    computePipelineLayout,
                                    0,
                                    1,
                                    &objectDescriptorSets[currentFrame],
                                    0,
                                    nullptr);

            ComputePushConstants compPc {};
            compPc.viewProj = viewProjection;

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

            compPc.objectCount = objectDataArray.size();
            vkCmdPushConstants(
                cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPc);

            uint32_t groupCount = (objectDataArray.size() + 255) / 256;
            vkCmdDispatch(cmd, groupCount, 1, 1);
        }
    }

    void CullPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }
} // namespace Engine
