#include "LightPassNode.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>
#include <future>
#include "Core/JobSystem.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    LightPassNode::LightPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap)
        : device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap)
    {
        objectDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        std::array<VkDescriptorSetLayoutBinding, 2> ssboBindings{};
        ssboBindings[0].binding = 0;
        ssboBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[0].descriptorCount = 1;
        ssboBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        ssboBindings[1].binding = 1;
        ssboBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ssboBindings[1].descriptorCount = 1;
        ssboBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout);

        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Renderer::MAX_FRAMES_IN_FLIGHT;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = Renderer::MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = Renderer::MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &objectDescriptorPool);

        const uint32_t MAX_SCENE_OBJECTS = 100000;

        cpuObjectSSBOs.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        cpuIndirectCommandBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        gpuObjectSSBOs.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        gpuIndirectCommandBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
           cpuObjectSSBOs[i] = std::make_unique<Buffer>(device, sizeof(ObjectData), MAX_SCENE_OBJECTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
           cpuIndirectCommandBuffers[i] =std::make_unique<Buffer>(device, sizeof(VkDrawIndexedIndirectCommand), MAX_SCENE_OBJECTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
           gpuObjectSSBOs[i] =std::make_unique<Buffer>(device, sizeof(ObjectData), MAX_SCENE_OBJECTS, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
           gpuIndirectCommandBuffers[i] =std::make_unique<Buffer>(device, sizeof(VkDrawIndexedIndirectCommand), MAX_SCENE_OBJECTS, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;

            vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]);

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = gpuObjectSSBOs[i]->getBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo indirectInfo{};
            indirectInfo.buffer = gpuIndirectCommandBuffers[i]->getBuffer();
            indirectInfo.offset = 0;
            indirectInfo.range = VK_WHOLE_SIZE;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
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
            descriptorWrites[1].pBufferInfo = &indirectInfo;

            vkUpdateDescriptorSets(device.getDevice(), 2, descriptorWrites.data(), 0, nullptr);
        }

        createPipelineLayout();
        createPipeline();
        createComputePipeline();
    }

    LightPassNode::~LightPassNode()
    {
        if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
        if (transparentPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), transparentPipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        if (graphicsPipelineDoubleSided != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), graphicsPipelineDoubleSided, nullptr);
        if (transparentPipelineDoubleSided != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), transparentPipelineDoubleSided, nullptr);
        if (objectDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device.getDevice(), objectDescriptorPool, nullptr);
        if (objectSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device.getDevice(), objectSetLayout, nullptr);
        if (computePipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), computePipeline, nullptr);
        if (computePipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
    }

    void LightPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();

        renderGraph.createTransientImage("G_AlbedoMetallic", VK_FORMAT_R8G8B8A8_UNORM, currentExtent);
        renderGraph.createTransientImage("G_NormalRoughness", VK_FORMAT_A2B10G10R10_UNORM_PACK32, currentExtent);

        renderGraph.writeImage("G_AlbedoMetallic", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        renderGraph.writeImage("G_NormalRoughness", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        renderGraph.writeImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    void LightPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PrepassPushConstants);

        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        VkDescriptorSetLayout layouts[] = {bindlessLayout, objectSetLayout};
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);
    }

    void LightPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/prepass.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/prepass.frag.spv");

        VkShaderModule vertShaderModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule fragShaderModule = ShaderUtils::createShaderModule(device.getDevice(), fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";

        auto bindingDescriptions = Model::VertexPosition::getBindingDescriptions();
        auto attributeDescriptions = Model::VertexPosition::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment[2]{};
        for(int i = 0; i < 2; i++) {
            colorBlendAttachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment[i].blendEnable = VK_FALSE;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments = colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormats[] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32 };

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 2;
        renderingCreateInfo.pColorAttachmentFormats = colorFormats;
        renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
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
        pipelineInfo.layout = pipelineLayout;

        vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

        depthStencil.depthWriteEnable = VK_FALSE;
        vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparentPipeline);

        rasterizer.cullMode = VK_CULL_MODE_NONE;
        depthStencil.depthWriteEnable = VK_TRUE;
        vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipelineDoubleSided);

        depthStencil.depthWriteEnable = VK_FALSE;
        vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparentPipelineDoubleSided);

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void LightPassNode::createComputePipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/cull.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo{};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &objectSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &computePipelineLayout);

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline);

        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }

    void LightPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        glm::mat4 projection = frameInfo.camera.getProjection();
        glm::mat4 view = frameInfo.camera.getView();
        glm::mat4 flipMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 viewProjection = projection * view * flipMatrix;

        if (sceneDirty)
        {
            glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
            opaqueDraws.clear(); transparentDraws.clear(); objectDataArray.clear(); indirectCommandsArray.clear(); drawBatches.clear();

            for (const auto& obj : frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0) continue;
                if (obj.alphaMode == AlphaMode::Blend) transparentDraws.push_back(&obj);
                else opaqueDraws.push_back(&obj);
            }

            std::sort(transparentDraws.begin(), transparentDraws.end(), [&camPos](const GameObject* a, const GameObject* b) {
                return glm::length(glm::vec3(a->currentWorldMatrix[3]) - camPos) > glm::length(glm::vec3(b->currentWorldMatrix[3]) - camPos);
            });

            std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&camPos](const GameObject* a, const GameObject* b) {
                if (a->doubleSided != b->doubleSided) return a->doubleSided < b->doubleSided;
                return glm::length(glm::vec3(a->currentWorldMatrix[3]) - camPos) < glm::length(glm::vec3(b->currentWorldMatrix[3]) - camPos);
            });

            auto buildBatches = [&](const std::vector<const GameObject*>& draws, bool isTransparent) {
                if (draws.empty()) return;

                DrawBatch currentBatch{};
                currentBatch.pipeline = draws[0]->doubleSided ? (isTransparent ? transparentPipelineDoubleSided : graphicsPipelineDoubleSided) : (isTransparent ? transparentPipeline : graphicsPipeline);
                currentBatch.firstCommandOffset = indirectCommandsArray.size();
                currentBatch.commandCount = 0;
                currentBatch.isTransparent = isTransparent ? 1 : 0;
                currentBatch.isDoubleSided = draws[0]->doubleSided ? 1 : 0;

                for (const auto* obj : draws) {
                    VkPipeline objPipeline = obj->doubleSided ? (isTransparent ? transparentPipelineDoubleSided : graphicsPipelineDoubleSided) : (isTransparent ? transparentPipeline : graphicsPipeline);
                    if (objPipeline != currentBatch.pipeline) {
                        drawBatches.push_back(currentBatch);
                        currentBatch.pipeline = objPipeline;
                        currentBatch.firstCommandOffset = indirectCommandsArray.size();
                        currentBatch.commandCount = 0;
                        currentBatch.isDoubleSided = obj->doubleSided ? 1 : 0;
                    }

                    ObjectData data;
                    data.modelMatrix = obj->currentWorldMatrix;
                    data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(obj->currentWorldMatrix))));
                    data.boundingSphere = obj->boundingSphere;
                    objectDataArray.push_back(data);

                    VkDrawIndexedIndirectCommand cmdCommand{};
                    cmdCommand.indexCount = obj->subMesh.indexCount;
                    cmdCommand.instanceCount = 0;
                    cmdCommand.firstIndex = obj->subMesh.firstIndex;
                    cmdCommand.vertexOffset = obj->subMesh.vertexOffset;
                    cmdCommand.firstInstance = objectDataArray.size() - 1;
                    indirectCommandsArray.push_back(cmdCommand);
                    currentBatch.commandCount++;
                }
                drawBatches.push_back(currentBatch);
            };

            buildBatches(opaqueDraws, false);
            buildBatches(transparentDraws, true);
            framesToUpdate = Renderer::MAX_FRAMES_IN_FLIGHT;
            sceneDirty = false;
        }

        if (framesToUpdate > 0)
        {
            if (!objectDataArray.empty()) {
                cpuObjectSSBOs[currentFrame]->writeToBuffer(objectDataArray.data(), objectDataArray.size() * sizeof(ObjectData), 0);
                cpuObjectSSBOs[currentFrame]->flush(VK_WHOLE_SIZE, 0);
                cpuIndirectCommandBuffers[currentFrame]->writeToBuffer(indirectCommandsArray.data(), indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand), 0);
                cpuIndirectCommandBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

                VkBufferCopy objCopy{}; objCopy.size = objectDataArray.size() * sizeof(ObjectData);
                vkCmdCopyBuffer(cmd, cpuObjectSSBOs[currentFrame]->getBuffer(), gpuObjectSSBOs[currentFrame]->getBuffer(), 1, &objCopy);

                VkBufferCopy indCopy{}; indCopy.size = indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand);
                vkCmdCopyBuffer(cmd, cpuIndirectCommandBuffers[currentFrame]->getBuffer(), gpuIndirectCommandBuffers[currentFrame]->getBuffer(), 1, &indCopy);

                std::array<VkBufferMemoryBarrier2, 2> copyBarriers{};
                copyBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                copyBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                copyBarriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                copyBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                copyBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                copyBarriers[0].buffer = gpuObjectSSBOs[currentFrame]->getBuffer();
                copyBarriers[0].offset = 0; copyBarriers[0].size = VK_WHOLE_SIZE;
                copyBarriers[1] = copyBarriers[0];
                copyBarriers[1].buffer = gpuIndirectCommandBuffers[currentFrame]->getBuffer();

                VkDependencyInfo copyDependency{};
                copyDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                copyDependency.bufferMemoryBarrierCount = 2;
                copyDependency.pBufferMemoryBarriers = copyBarriers.data();
                vkCmdPipelineBarrier2(cmd, &copyDependency);
            }
            framesToUpdate--;
        }

        if (!objectDataArray.empty())
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &objectDescriptorSets[currentFrame], 0, nullptr);

            ComputePushConstants compPc{};
            compPc.viewProjection = viewProjection;
            compPc.objectCount = objectDataArray.size();
            vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPc);

            uint32_t groupCount = (objectDataArray.size() + 255) / 256;
            vkCmdDispatch(cmd, groupCount, 1, 1);

            VkBufferMemoryBarrier2 indirectBarrier{};
            indirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            indirectBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            indirectBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
            indirectBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            indirectBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            indirectBarrier.buffer = gpuIndirectCommandBuffers[currentFrame]->getBuffer();
            indirectBarrier.offset = 0; indirectBarrier.size = VK_WHOLE_SIZE;

            VkDependencyInfo computeDependency{};
            computeDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            computeDependency.bufferMemoryBarrierCount = 1;
            computeDependency.pBufferMemoryBarriers = &indirectBarrier;
            vkCmdPipelineBarrier2(cmd, &computeDependency);
        }

        if (!objectDataArray.empty())
        {
            VkRenderingAttachmentInfo albedoAttachment{};
            albedoAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            albedoAttachment.imageView = frameInfo.renderGraph->getImageView("G_AlbedoMetallic");
            albedoAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            albedoAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

            VkRenderingAttachmentInfo normalAttachment{};
            normalAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            normalAttachment.imageView = frameInfo.renderGraph->getImageView("G_NormalRoughness");
            normalAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            normalAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = renderer.getSwapChain().getDepthImageView();
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingAttachmentInfo colorAttachments[] = { albedoAttachment, normalAttachment };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = {0, 0};
            renderingInfo.renderArea.extent = frameInfo.extent;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 2; // TWO COLOR ATTACHMENTS
            renderingInfo.pColorAttachments = colorAttachments;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vkCmdBeginRendering(cmd, &renderingInfo);

            VkViewport viewport{};
            viewport.width = static_cast<float>(frameInfo.extent.width);
            viewport.height = static_cast<float>(frameInfo.extent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.extent = frameInfo.extent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDescriptorSet globalBindlessSet = resourceHeap.getDescriptorSet();
            VkDescriptorSet sets[] = {globalBindlessSet, objectDescriptorSets[currentFrame]};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &viewProjection);
            megaBuffer.bind(cmd);

            VkPipeline currentPipeline = VK_NULL_HANDLE;

            for (const auto& batch : drawBatches)
            {
                if (batch.isTransparent) continue;

                if (currentPipeline != batch.pipeline) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.pipeline);
                    currentPipeline = batch.pipeline;
                }

                vkCmdDrawIndexedIndirect(
                    cmd,
                    gpuIndirectCommandBuffers[currentFrame]->getBuffer(),
                    batch.firstCommandOffset * sizeof(VkDrawIndexedIndirectCommand),
                    batch.commandCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );
            }
            vkCmdEndRendering(cmd);
        }
    }
}