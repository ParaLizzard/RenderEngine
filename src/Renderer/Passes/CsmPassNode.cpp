#include "CsmPassNode.h"

#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Vulkan/ResourceHeap.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    CsmPassNode::CsmPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap):device(device),renderer(renderer), megaBuffer(megaBuffer),resourceHeap(resourceHeap)
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
        gpuObjectSSBOs.resize(Config::MAX_FRAMES_IN_FLIGHT);
        cpuIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
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
                                         Config::MAX_SCENE_OBJECTS * SHADOW_MAP_CASCADES,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         0);

            gpuDrawCountBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(uint32_t),
                                         SHADOW_MAP_CASCADES,
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

        createPipelineLayout();
        createPipeline();
    }

    CsmPassNode::~CsmPassNode()
    {
        if (objectDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device.getDevice(), objectDescriptorPool, nullptr);
        if (objectSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device.getDevice(), objectSetLayout, nullptr);
        if (computePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), computePipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void CsmPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D mapExtent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        renderGraph.createTransientImage(
            "CsmImage",
            VK_FORMAT_D32_SFLOAT,
            mapExtent,
            SHADOW_MAP_CASCADES,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            );

        renderGraph.writeImage(
            "CsmImage",
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
            );
    }

    void CsmPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        if (sceneDirty) {
            opaqueDraws.clear();
            objectDataArray.clear();
            indirectCommandsArray.clear();

            for (const auto &obj: *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0) continue;
                if (obj.alphaMode == AlphaMode::Blend) continue;
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
                cmdCommand.firstInstance = static_cast<uint32_t>(objectDataArray.size() - 1);
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
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
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
            vkCmdFillBuffer(
                cmd, gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, sizeof(uint32_t) * SHADOW_MAP_CASCADES, 0);

            VkBufferMemoryBarrier2 fillBarrier = VkUtils::bufferBarrier(
                gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDependencyInfo fillDep {};
            fillDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            fillDep.bufferMemoryBarrierCount = 1;
            fillDep.pBufferMemoryBarriers = &fillBarrier;
            vkCmdPipelineBarrier2(cmd, &fillDep);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    computePipelineLayout,
                                    0,
                                    1,
                                    &objectDescriptorSets[currentFrame],
                                    0,
                                    nullptr);

            for (uint32_t cascadeIndex = 0; cascadeIndex < SHADOW_MAP_CASCADES; cascadeIndex++) {
                ComputePushConstants compPc {};
                compPc.viewProj = cascadeViewProjs[cascadeIndex];

                glm::mat4 tvp = glm::transpose(compPc.viewProj);
                compPc.frustumPlanes[0] = tvp[3] + tvp[0];
                compPc.frustumPlanes[1] = tvp[3] - tvp[0];
                compPc.frustumPlanes[2] = tvp[3] + tvp[1];
                compPc.frustumPlanes[3] = tvp[3] - tvp[1];
                compPc.frustumPlanes[4] = tvp[2];
                compPc.frustumPlanes[5] = tvp[3] - tvp[2];

                for (int i = 0; i < 6; i++) {
                    float len = glm::length(glm::vec3(compPc.frustumPlanes[i]));
                    compPc.frustumPlanes[i] /= len;
                }

                compPc.objectCount = static_cast<uint32_t>(objectDataArray.size());
                compPc.cascadeIndex = cascadeIndex;
                compPc.objectCapacity = Config::MAX_SCENE_OBJECTS;
                compPc.clipPlaneCount = 4;
                vkCmdPushConstants(
                    cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPc);

                uint32_t groupCount = (static_cast<uint32_t>(objectDataArray.size()) + 255) / 256;
                vkCmdDispatch(cmd, groupCount, 1, 1);
            }

            VkBufferMemoryBarrier2 computeBarrier = VkUtils::bufferBarrier(
                gpuCompactedIndirectCommandBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

            VkBufferMemoryBarrier2 countBarrier = VkUtils::bufferBarrier(
                gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

            std::array<VkBufferMemoryBarrier2, 2> computeBarriers = {computeBarrier, countBarrier};
            VkDependencyInfo computeDep {};
            computeDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            computeDep.bufferMemoryBarrierCount = 2;
            computeDep.pBufferMemoryBarriers = computeBarriers.data();
            vkCmdPipelineBarrier2(cmd, &computeDep);
        }

        for (uint32_t cascadeIndex = 0; cascadeIndex < SHADOW_MAP_CASCADES; cascadeIndex++) {
            VkRenderingAttachmentInfo depthAttachment {};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = cascadeViews[cascadeIndex];
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 0;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vkCmdBeginRendering(cmd, &renderingInfo);

            VkViewport viewport {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(SHADOW_MAP_SIZE);
            viewport.height = static_cast<float>(SHADOW_MAP_SIZE);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor {};
            scissor.offset = {0, 0};
            scissor.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
            VkDescriptorSet sets[] = {bindlessSet, objectDescriptorSets[currentFrame]};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

            CsmPassPushConstants pc {};
            pc.cascadeIndex = cascadeIndex;
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CsmPassPushConstants), &pc);

            megaBuffer.bindPositionOnly(cmd);

            if (!objectDataArray.empty()) {
                VkDeviceSize compactedOffset = static_cast<VkDeviceSize>(cascadeIndex) *
                    Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand);
                VkDeviceSize countOffset = static_cast<VkDeviceSize>(cascadeIndex) * sizeof(uint32_t);

                vkCmdDrawIndexedIndirectCount(cmd,
                                              gpuCompactedIndirectCommandBuffers[currentFrame]->getBuffer(),
                                              compactedOffset,
                                              gpuDrawCountBuffers[currentFrame]->getBuffer(),
                                              countOffset,
                                              static_cast<uint32_t>(objectDataArray.size()),
                                              sizeof(VkDrawIndexedIndirectCommand));
            }

            vkCmdEndRendering(cmd);
        }
    }

    void CsmPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        VkImage currentImage = graph.getImage("CsmImage");
        if (currentImage != VK_NULL_HANDLE && currentImage != csmImageCache) {
            for (uint32_t i = 0; i < SHADOW_MAP_CASCADES; i++) {
                if (cascadeViews[i] != VK_NULL_HANDLE) {
                    vkDestroyImageView(device.getDevice(), cascadeViews[i], nullptr);
                }

                VkImageViewCreateInfo viewInfo {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = currentImage;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = VK_FORMAT_D32_SFLOAT;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = i;
                viewInfo.subresourceRange.layerCount = 1;

                vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &cascadeViews[i]);
            }
            csmImageCache = currentImage;
        }
    }


    void CsmPassNode::createPipelineLayout()
    {
        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();
        VkDescriptorSetLayout layouts[] = {bindlessLayout, objectSetLayout};

        VkPushConstantRange pushConstantRangeGraphics {};
        pushConstantRangeGraphics.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRangeGraphics.offset = 0;
        pushConstantRangeGraphics.size = sizeof(CsmPassPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRangeGraphics;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);

        VkPushConstantRange pushConstantRangeCompute {};
        pushConstantRangeCompute.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRangeCompute.offset = 0;
        pushConstantRangeCompute.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutComputeInfo {};
        pipelineLayoutComputeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutComputeInfo.setLayoutCount = 1;
        pipelineLayoutComputeInfo.pSetLayouts = &objectSetLayout;
        pipelineLayoutComputeInfo.pushConstantRangeCount = 1;
        pipelineLayoutComputeInfo.pPushConstantRanges = &pushConstantRangeCompute;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutComputeInfo, nullptr, &computePipelineLayout);
    }

    void CsmPassNode::createPipeline()
    {
        // depth only

        auto vertCode = ShaderUtils::readFile("shaders/shadow.vert.spv");
        auto compCode = ShaderUtils::readFile("shaders/cull.comp.spv");

        VkShaderModule vertShaderModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo shaderStages[1] {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        VkPipelineShaderStageCreateInfo computeStageInfo {};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";



        VkVertexInputBindingDescription binding {};
        binding.binding = 0;
        binding.stride = sizeof(Model::VertexPosition);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attribute {};
        attribute.binding = 0;
        attribute.location = 0;
        attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute.offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &binding;
        vertexInputInfo.vertexAttributeDescriptionCount = 1;
        vertexInputInfo.pVertexAttributeDescriptions = &attribute;

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
        rasterizer.depthClampEnable = VK_FALSE;

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
        renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

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

        VkComputePipelineCreateInfo pipelineComputeInfo {};
        pipelineComputeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineComputeInfo.layout = computePipelineLayout;
        pipelineComputeInfo.stage = computeStageInfo;

        vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineComputeInfo, nullptr, &computePipeline);
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }

    void CsmPassNode::updateCascades(SceneUbo &sceneUbo, FrameInfo &frameInfo)
    {
        float nearClip = frameInfo.camera->getNearClip();
        float farClip = frameInfo.camera->getFarClip();
        float clipRange = farClip - nearClip;

        float minZ = nearClip;
        float maxZ = nearClip + clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        float cascadeSplitLambda = 0.95f;

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

            // Project frustum corners into world
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

            // Frustum centre
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

            glm::vec3 lightDir = glm::normalize(glm::vec3(-sceneUbo.directionalLight));
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(lightDir.y) > 0.999f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            float zMultiplier = 300.0f;

            float nearPlane = 0.01f;
            float farPlane  = zMultiplier + radius * 1.5f;

            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter + lightDir * zMultiplier, frustumCenter, up);
            glm::mat4 lightOrthoMatrix = glm::orthoZO(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, nearPlane, farPlane);

            lightOrthoMatrix[1][1] *= -1.0f;

            glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
            glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            float shadowMapSize = static_cast<float>(SHADOW_MAP_SIZE);
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