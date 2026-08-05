#include "CsmPassNode.h"

#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Vulkan/ResourceHeap.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    CsmPassNode::CsmPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, CullPassNode &cullPass)
        : device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap), cullPass(cullPass)
    {
        objectDescriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        // --- Descriptor set layout: 4 SSBO bindings (0-3) for CSM storage ---
        std::array<VkDescriptorSetLayoutBinding, 4> ssboBindings {};
        for (uint32_t b = 0; b < 4; b++) {
            ssboBindings[b].binding = b;
            ssboBindings[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ssboBindings[b].descriptorCount = 1;
            ssboBindings[b].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout);

        // --- Descriptor pool ---
        std::array<VkDescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = Config::MAX_FRAMES_IN_FLIGHT * 4;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT;
        vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &objectDescriptorPool);

        gpuCompactedIndirectCommandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        gpuDrawCountBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        cascadeDataBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
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
    }

    CsmPassNode::~CsmPassNode()
    {
        if (csmArrayView != VK_NULL_HANDLE)
            vkDestroyImageView(device.getDevice(), csmArrayView, nullptr);
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
            Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT,
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

    void CsmPassNode::updateDescriptors()
    {
        for (uint32_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo blueprintInfo {};
            blueprintInfo.buffer = cullPass.getGpuIndirectCommandBuffer(i);
            blueprintInfo.offset = 0;
            blueprintInfo.range = VK_WHOLE_SIZE;

            VkDescriptorBufferInfo countInfo = gpuDrawCountBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compactedInfo = gpuCompactedIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo cascadeInfo = cascadeDataBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 4> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = objectDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &blueprintInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = objectDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &countInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = objectDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &compactedInfo;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = objectDescriptorSets[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pBufferInfo = &cascadeInfo;

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
        uint32_t objectCount = cullPass.getActualObjectCount();

        if (objectCount > 0) {
            CascadeGpuData cascadeData {};
            for (uint32_t c = 0; c < SHADOW_MAP_CASCADES; c++) {
                cascadeData.viewProj[c] = cascadeViewProjs[c];

                glm::mat4 tvp = glm::transpose(cascadeViewProjs[c]);
                glm::vec4 planes[6];
                planes[0] = tvp[3] + tvp[0]; // Left
                planes[1] = tvp[3] - tvp[0]; // Right
                planes[2] = tvp[3] + tvp[1]; // Bottom
                planes[3] = tvp[3] - tvp[1]; // Top
                planes[4] = tvp[2];           // Near
                planes[5] = tvp[3] - tvp[2];  // Far

                for (int p = 0; p < 6; p++) {
                    float len = glm::length(glm::vec3(planes[p]));
                    cascadeData.frustumPlanes[c * 6 + p] = planes[p] / len;
                }
            }
            cascadeDataBuffers[currentFrame]->writeToBuffer(&cascadeData, sizeof(CascadeGpuData), 0);
            cascadeDataBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

            vkCmdFillBuffer(cmd, gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE, 0);

            VkBufferMemoryBarrier2 barriers[2];
            barriers[0] = VkUtils::bufferBarrier(
                cascadeDataBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

            barriers[1] = VkUtils::bufferBarrier(
                gpuDrawCountBuffers[currentFrame]->getBuffer(), 0, VK_WHOLE_SIZE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDependencyInfo depInfo {};
            depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depInfo.bufferMemoryBarrierCount = 2;
            depInfo.pBufferMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(cmd, &depInfo);

            VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
            VkDescriptorSet computeSets[] = {bindlessSet, objectDescriptorSets[currentFrame]};

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 2, computeSets, 0, nullptr);

            CsmCullPushConstants compPc {};
            compPc.objectCount = objectCount;
            compPc.objectCapacity = Config::MAX_SCENE_OBJECTS;
            vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CsmCullPushConstants), &compPc);

            uint32_t groupCountX = (objectCount + 63) / 64;
            vkCmdDispatch(cmd, groupCountX, 1, 1);

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

        VkRenderingAttachmentInfo depthAttachment {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = csmArrayView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo renderingInfo {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        renderingInfo.layerCount = SHADOW_MAP_CASCADES;
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

        megaBuffer.bindPositionOnly(cmd);

        if (objectCount > 0) {
            for (uint32_t c = 0; c < SHADOW_MAP_CASCADES; c++) {
                uint32_t cascadeIndex = c;
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &cascadeIndex);

                VkDeviceSize commandOffset = c * Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand);
                VkDeviceSize countOffset = c * sizeof(uint32_t);

                vkCmdDrawIndexedIndirectCount(cmd,
                                              gpuCompactedIndirectCommandBuffers[currentFrame]->getBuffer(),
                                              commandOffset,
                                              gpuDrawCountBuffers[currentFrame]->getBuffer(),
                                              countOffset,
                                              objectCount,
                                              sizeof(VkDrawIndexedIndirectCommand));
            }
        }

        vkCmdEndRendering(cmd);
    }

    void CsmPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        VkImage currentImage = graph.getImage("CsmImage");
        if (currentImage != VK_NULL_HANDLE && currentImage != csmImageCache) {
            if (csmArrayView != VK_NULL_HANDLE) {
                vkDestroyImageView(device.getDevice(), csmArrayView, nullptr);
            }

            VkImageViewCreateInfo viewInfo {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = currentImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            viewInfo.format = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = SHADOW_MAP_CASCADES;

            vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &csmArrayView);
            csmImageCache = currentImage;
        }
    }

    void CsmPassNode::createPipelineLayout()
    {
        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();
        VkDescriptorSetLayout layouts[] = {bindlessLayout, objectSetLayout};

        VkPushConstantRange graphicsPushConstant {};
        graphicsPushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        graphicsPushConstant.offset = 0;
        graphicsPushConstant.size = sizeof(uint32_t); // cascadeIndex

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &graphicsPushConstant;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);

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
        auto compCode = ShaderUtils::readFile("shaders/csm_cull.comp.spv");

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
        renderingCreateInfo.depthAttachmentFormat = Config::USE_D16_SHADOW_MAPS ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT;

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

            glm::vec3 lightDir = glm::normalize(glm::vec3(sceneUbo.directionalLight));
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(lightDir.y) > 0.999f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            float zMultiplier = 100.0f;

            float nearPlane = 0.0f;
            float farPlane  = zMultiplier + radius * 2.0f;

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