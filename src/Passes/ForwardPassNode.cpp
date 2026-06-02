#include "ForwardPassNode.h"
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
    ForwardPassNode::ForwardPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap)
        : device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap)
    {
        createPipelineLayout();
        createPipeline();
    }

    ForwardPassNode::~ForwardPassNode()
    {
        if (graphicsPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
        }
        if (transparentPipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.getDevice(), transparentPipeline, nullptr);
        }
        if (pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        }
        if (graphicsPipelineDoubleSided != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.getDevice(), graphicsPipelineDoubleSided, nullptr);
        }
        if (transparentPipelineDoubleSided != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device.getDevice(), transparentPipelineDoubleSided, nullptr);
        }
    }

    void ForwardPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        //VkFormat currentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat currentFormat = renderer.getSwapChain().getSwapChainImageFormat();

        renderGraph.createTransientImage("SceneColorImage", currentFormat, currentExtent);


        renderGraph.readBuffer("SceneUBO",
                               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                               VK_ACCESS_2_UNIFORM_READ_BIT);
        renderGraph.readBuffer("MaterialSSBO",
                               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        renderGraph.writeImage("SceneColorImage", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        renderGraph.writeImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    void ForwardPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ForwardPushConstants);

        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &bindlessLayout;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout");
        }
    }

    void ForwardPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/pbr.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/pbr.frag.spv");

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

        auto bindingDescriptions = Model::Vertex::getBindingDescriptions();
        auto attributeDescriptions = Model::Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        //VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat colorFormat = renderer.getSwapChain().getSwapChainImageFormat();
        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.pNext = nullptr;
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &colorFormat;
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
        pipelineInfo.renderPass = VK_NULL_HANDLE;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline");
        }

        depthStencil.depthWriteEnable = VK_TRUE;
        //depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &transparentPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create transparent graphics pipeline");
        }

        rasterizer.cullMode = VK_CULL_MODE_NONE;

        depthStencil.depthWriteEnable = VK_TRUE;
        colorBlendAttachment.blendEnable = VK_FALSE;
        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipelineDoubleSided) != VK_SUCCESS) {
            throw std::runtime_error("failed to create double-sided graphics pipeline");
        }

        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparentPipelineDoubleSided) != VK_SUCCESS) {
            throw std::runtime_error("failed to create double-sided transparent pipeline");
        }

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void ForwardPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = frameInfo.renderGraph->getImageView("SceneColorImage");
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.02f, 0.02f, 0.02f, 1.0f}};

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = renderer.getSwapChain().getDepthImageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = frameInfo.extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(frameInfo.extent.width);
        viewport.height = static_cast<float>(frameInfo.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = frameInfo.extent;

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDescriptorSet globalBindlessSet = resourceHeap.getDescriptorSet();
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &globalBindlessSet,
            0, nullptr
        );

        glm::mat4 projection = frameInfo.camera.getProjection();
        glm::mat4 view = frameInfo.camera.getView();
        glm::mat4 viewProjection = projection * view;

        glm::mat4 tvp = glm::transpose(viewProjection);

        std::array<glm::vec4, 6> frustumPlanes;
        frustumPlanes[0] = tvp[3] + tvp[0]; // Left
        frustumPlanes[1] = tvp[3] - tvp[0]; // Right
        frustumPlanes[2] = tvp[3] + tvp[1]; // Bottom
        frustumPlanes[3] = tvp[3] - tvp[1]; // Top
        // 2. VULKAN SPECIFIC: Depth is 0 to 1, so Near plane is just Row 2
        frustumPlanes[4] = tvp[2];          // Near
        frustumPlanes[5] = tvp[3] - tvp[2]; // Far

        for (int i = 0; i < 6; i++) {
            float length = glm::length(glm::vec3(frustumPlanes[i]));
            frustumPlanes[i] /= length;
        }

        glm::mat4 flipMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        std::vector<const GameObject*> opaqueDraws;
        std::vector<const GameObject*> transparentDraws;

        struct CullResult {
            std::vector<const GameObject*> localOpaque;
            std::vector<const GameObject*> localTransparent;
        };

        size_t objectCount = frameInfo.gameObjects.size();
        size_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
        size_t itemsPerChunk = (objectCount + numThreads - 1) / numThreads; // Ceiling division

        std::vector<std::future<CullResult>> futures;

        for (size_t i = 0; i < numThreads; ++i) {
            size_t startIdx = i * itemsPerChunk;
            size_t endIdx = std::min(startIdx + itemsPerChunk, objectCount);

            if (startIdx >= endIdx) break;

            futures.push_back(frameInfo.jobSystem->enqueue([startIdx, endIdx, &frameInfo, flipMatrix, frustumPlanes]() {
                CullResult result;
                result.localOpaque.reserve(endIdx - startIdx);
                result.localTransparent.reserve(endIdx - startIdx);

                for (size_t j = startIdx; j < endIdx; ++j) {
                    const auto& obj = frameInfo.gameObjects[j];
                    if (obj.subMesh.indexCount == 0) continue;

                    glm::mat4 actualWorldMatrix = flipMatrix * obj.currentWorldMatrix;
                    glm::vec3 worldCenter = glm::vec3(actualWorldMatrix * glm::vec4(obj.boundingSphere.x, obj.boundingSphere.y, obj.boundingSphere.z, 1.0f));

                    float scaleX = glm::length(glm::vec3(actualWorldMatrix[0]));
                    float scaleY = glm::length(glm::vec3(actualWorldMatrix[1]));
                    float scaleZ = glm::length(glm::vec3(actualWorldMatrix[2]));
                    float maxScale = std::max({scaleX, scaleY, scaleZ});
                    float worldRadius = obj.boundingSphere.w * maxScale;

                    bool visible = true;
                    for (int p = 0; p < 6; p++) {
                        if (glm::dot(glm::vec3(frustumPlanes[p]), worldCenter) + frustumPlanes[p].w < -worldRadius) {
                            visible = false;
                            break;
                        }
                    }

                    if (!visible) continue;

                    if (obj.alphaMode == AlphaMode::Blend) result.localTransparent.push_back(&obj);
                    else result.localOpaque.push_back(&obj);
                }
                return result;
            }));
        }

        for (auto& future : futures) {
            CullResult res = future.get();
            opaqueDraws.insert(opaqueDraws.end(), res.localOpaque.begin(), res.localOpaque.end());
            transparentDraws.insert(transparentDraws.end(), res.localTransparent.begin(), res.localTransparent.end());
        }

        glm::vec3 camPos = glm::vec3(glm::inverse(frameInfo.camera.getView())[3]);

        std::sort(transparentDraws.begin(), transparentDraws.end(), [&camPos](const GameObject* a, const GameObject* b)
        {
            float distA = glm::length(glm::vec3(a->currentWorldMatrix[3]) - camPos);
            float distB = glm::length(glm::vec3(b->currentWorldMatrix[3]) - camPos);
            return distA > distB;
        });

        std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&camPos](const GameObject* a, const GameObject* b)
        {
            float distA = glm::length(glm::vec3(a->currentWorldMatrix[3]) - camPos);
            float distB = glm::length(glm::vec3(b->currentWorldMatrix[3]) - camPos);
            return distA < distB; // Nearest first
        });

        uint32_t lastBoundChunk = 999999;

        VkPipeline currentPipeline = VK_NULL_HANDLE;

        auto bindPipeline = [&](VkPipeline newPipeline) {
            if (currentPipeline != newPipeline) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, newPipeline);
                currentPipeline = newPipeline;
            }
        };

        for (const auto* obj : opaqueDraws) {
            bindPipeline(obj->doubleSided ? graphicsPipelineDoubleSided : graphicsPipeline);

            if (obj->subMesh.bufferIndex != lastBoundChunk) {
                megaBuffer.bind(cmd, obj->subMesh.bufferIndex);
                lastBoundChunk = obj->subMesh.bufferIndex;
            }
            ForwardPushConstants pushConstants{};
            pushConstants.modelMatrix = flipMatrix * obj->currentWorldMatrix;
            pushConstants.viewProjection = viewProjection;
            pushConstants.debugIsTransparent = 0;
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ForwardPushConstants), &pushConstants);
            vkCmdDrawIndexed(cmd, obj->subMesh.indexCount, 1, obj->subMesh.firstIndex, obj->subMesh.vertexOffset, 0);
        }

        if (!transparentDraws.empty()) {
            for (const auto* obj : transparentDraws) {
                bindPipeline(obj->doubleSided ? transparentPipelineDoubleSided : transparentPipeline);

                if (obj->subMesh.bufferIndex != lastBoundChunk) {
                    megaBuffer.bind(cmd, obj->subMesh.bufferIndex);
                    lastBoundChunk = obj->subMesh.bufferIndex;
                }
                ForwardPushConstants pushConstants{};
                pushConstants.modelMatrix = flipMatrix * obj->currentWorldMatrix;
                pushConstants.viewProjection = viewProjection;
                pushConstants.debugIsTransparent = 1;
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ForwardPushConstants), &pushConstants);
                vkCmdDrawIndexed(cmd, obj->subMesh.indexCount, 1, obj->subMesh.firstIndex, obj->subMesh.vertexOffset, 0);
            }
        }
        vkCmdEndRendering(cmd);
    }
}
