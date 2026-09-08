#include "Renderer/Passes/VisibilityPassNode.h"
#include "Core/Assert.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ShaderUtils.h"
#include "Renderer/Passes/CullPassNode.h"
#include "Core/EngineConstants.h"

namespace Engine {
    VisibilityPassNode::VisibilityPassNode(Device &device,
                                           Renderer &renderer,
                                           Model &megaBuffer,
                                           CullPassNode &cullPass,
                                           ResourceHeap &resourceHeap,
                                           uint32_t phase):
        RenderPassNode(phase == 0 ? "Visibility Pass Phase 1" : "Visibility Pass Phase 2"), device(device), renderer(renderer), megaBuffer(megaBuffer), cullPass(cullPass), resourceHeap(resourceHeap), phase(phase)
    {
        createPipelineLayout();
        createPipeline();
        createMaskedPipeline();
        
        if (device.isMeshShaderSupported()) {
            pfn_vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkGetInstanceProcAddr(device.getInstance(), "vkCmdDrawMeshTasksEXT");
            pfn_vkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetInstanceProcAddr(device.getInstance(), "vkCmdDrawMeshTasksIndirectEXT");
            ENGINE_VERIFY(pfn_vkCmdDrawMeshTasksEXT != nullptr && pfn_vkCmdDrawMeshTasksIndirectEXT != nullptr,
                "Failed to load mesh shader extension functions");
            createMeshPipeline();
        }
    }

    VisibilityPassNode::~VisibilityPassNode()
    {
        if (meshPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), meshPipeline, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        if (meshPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), meshPipelineLayout, nullptr);
        if (maskedPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), maskedPipeline, nullptr);
        if (maskedPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), maskedPipelineLayout, nullptr);
    }

    void VisibilityPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();

        if (phase == 0) {
            renderGraph.createTransientImage("VisBuffer",
                                             VK_FORMAT_R32G32_UINT,
                                             currentExtent);
            renderGraph.createTransientImage("VelocityBuffer",
                                             VK_FORMAT_R16G16_SFLOAT,
                                             currentExtent,
                                             1,
                                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        }

        if (!device.isMeshShaderSupported()) {
            renderGraph.readBuffer("CompactedIndexBuffer",
                                   VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_READ_BIT);

            renderGraph.readBuffer("SingleIndirectCommand",
                                   VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                   VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

            renderGraph.readBuffer("MaskedCompactedIndexBuffer",
                                   VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_READ_BIT);

            renderGraph.readBuffer("MaskedSingleIndirectCommand",
                                   VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                                   VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        }

        renderGraph.writeImage("VisBuffer",
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        renderGraph.writeImage("VelocityBuffer",
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        renderGraph.writeImage(
            "DepthImage",
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }

    void VisibilityPassNode::registerResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        if (phase == 0) {
            VkExtent2D currentExtent = frameInfo.extent;
            graph.registerPhysicalImage("DepthImage",
                                        renderer.getSwapChain().getDepthImage(),
                                        renderer.getSwapChain().getDepthImageView(),
                                        renderer.getSwapChain().getDepthFormat(),
                                        currentExtent,
                                        VK_IMAGE_LAYOUT_UNDEFINED);
        }
    }

    void VisibilityPassNode::updateResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        if (phase == 0) {
            VkExtent2D currentExtent = frameInfo.extent;
            graph.updateImageHandle("DepthImage",
                                    renderer.getSwapChain().getDepthImage(),
                                    renderer.getSwapChain().getDepthImageView(),
                                    currentExtent);
        }
    }

    void VisibilityPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        glm::mat4 proj = frameInfo.camera->getProjection();
        VkExtent2D hizExt = { std::bit_ceil(frameInfo.extent.width), std::bit_ceil(frameInfo.extent.height) };
        uint32_t totalMips = std::bit_width(std::max(hizExt.width, hizExt.height));

        VisibilityPushConstants pushConsts{};
        pushConsts.view               = frameInfo.cullView;
        pushConsts.projParams         = glm::vec4(proj[0][0], proj[1][1], proj[2][2], proj[3][2]);
        pushConsts.hizParams          = glm::vec4((frameInfo.extent.width * 0.5f) / hizExt.width,
                                                  (frameInfo.extent.height * 0.5f) / hizExt.height,
                                                  static_cast<float>(totalMips),
                                                  frameInfo.camera->getNearClip());
        pushConsts.screenParams       = glm::vec2(static_cast<float>(frameInfo.extent.width), static_cast<float>(frameInfo.extent.height));
        pushConsts.cullFlags          = 0;
        if (frameInfo.cullEnabled) pushConsts.cullFlags |= 1u;
        if (frameInfo.cullEnabled) pushConsts.cullFlags |= 2u;
        if (frameInfo.cullEnabled && !frameInfo.firstFrame && phase == 1) pushConsts.cullFlags |= 4u;
        pushConsts.objectCount        = megaBuffer.getMeshletCount();
        pushConsts.actualObjectCount  = static_cast<uint32_t>(frameInfo.gameObjects->size());
        pushConsts.objectCapacity     = Constants::MAX_SCENE_OBJECTS;
        pushConsts.clipPlaneCount     = 6;
        pushConsts.isMeshShader       = device.isMeshShaderSupported() ? 1 : 0;
        pushConsts.phase              = phase;

        VkAttachmentLoadOp loadOp = (phase == 0) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

        VkRenderingAttachmentInfo colorAttachment {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = frameInfo.renderGraph->getImageView("VisBuffer");
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = loadOp;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.uint32[0] = 0;
        colorAttachment.clearValue.color.uint32[1] = 0;

        VkRenderingAttachmentInfo velocityAttachment {};
        velocityAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        velocityAttachment.imageView = frameInfo.renderGraph->getImageView("VelocityBuffer");
        velocityAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        velocityAttachment.loadOp = loadOp;
        velocityAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        velocityAttachment.clearValue.color.float32[0] = 0.0f;
        velocityAttachment.clearValue.color.float32[1] = 0.0f;

        VkRenderingAttachmentInfo depthAttachment {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = renderer.getSwapChain().getDepthImageView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = loadOp;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        std::array attachments { colorAttachment, velocityAttachment };
        VkRenderingInfo renderingInfo {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = frameInfo.extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 2;
        renderingInfo.pColorAttachments = attachments.data();
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport {};
        viewport.width = static_cast<float>(frameInfo.extent.width);
        viewport.height = static_cast<float>(frameInfo.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.extent = frameInfo.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        if (device.isMeshShaderSupported()) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

            VkDescriptorSet sets[] = {resourceHeap.getDescriptorSet(currentFrame), cullPass.getObjectDescriptorSet(currentFrame)};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout, 0, 2, sets, 0, nullptr);

            vkCmdPushConstants(cmd,
                               meshPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT,
                               0,
                               sizeof(VisibilityPushConstants),
                               &pushConsts);

            if (pushConsts.actualObjectCount > 0) {
                pfn_vkCmdDrawMeshTasksIndirectEXT(cmd, cullPass.getTaskDispatchCommandBuffer(currentFrame), 0, 1, sizeof(VkDispatchIndirectCommand));
            }

        } else {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            VkDescriptorSet sets[] = {resourceHeap.getDescriptorSet(currentFrame), cullPass.getObjectDescriptorSet(currentFrame)};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

            vkCmdPushConstants(cmd,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(VisibilityPushConstants),
                               &pushConsts);

            vkCmdDrawIndirect(cmd,
                              cullPass.getSingleIndirectCommandBuffer(currentFrame),
                              0,
                              1,
                              sizeof(VkDrawIndirectCommand));
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, maskedPipeline);

        VkDescriptorSet maskedSets[] = {
            resourceHeap.getDescriptorSet(currentFrame),
            cullPass.getMaskedObjectDescriptorSet(currentFrame)
        };
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                maskedPipelineLayout,
                                0,
                                2,
                                maskedSets,
                                0,
                                nullptr);

        vkCmdPushConstants(cmd,
                           maskedPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(VisibilityPushConstants),
                           &pushConsts);

        vkCmdDrawIndirect(cmd,
                          cullPass.getMaskedSingleIndirectCommandBuffer(currentFrame),
                          0,
                          1,
                          sizeof(VkDrawIndirectCommand));

        vkCmdEndRendering(cmd);
    }

    void VisibilityPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }

    void VisibilityPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(VisibilityPushConstants);

        VkDescriptorSetLayout layouts[] = {resourceHeap.getDescriptorSetLayout(), cullPass.getObjectSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        ENGINE_VERIFY(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS,
            "VisibilityPassNode: failed to create pipeline layout");
    }

    void VisibilityPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/visbuffer.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/visbuffer.frag.spv");

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

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments{};
        blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[0].blendEnable = VK_FALSE;
        blendAttachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[1].blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments = blendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        std::array formats = {VK_FORMAT_R32G32_UINT, VK_FORMAT_R16G16_SFLOAT};
        VkPipelineRenderingCreateInfo renderingCreateInfo {};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 2;
        renderingCreateInfo.pColorAttachmentFormats = formats.data();
        renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

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
        pipelineInfo.layout = pipelineLayout;

        vkCreateGraphicsPipelines(
            device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &pipeline);

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void VisibilityPassNode::createMeshPipeline()
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(VisibilityPushConstants);

        VkDescriptorSetLayout layouts[] = {resourceHeap.getDescriptorSetLayout(), cullPass.getObjectSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &meshPipelineLayout);

        auto taskCode = ShaderUtils::readFile("shaders/meshlet.task.spv");
        auto meshCode = ShaderUtils::readFile("shaders/triangle.mesh.spv");
        auto fragCode = ShaderUtils::readFile("shaders/visbuffer_mesh.frag.spv");

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

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments{};
        blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[0].blendEnable = VK_FALSE;
        blendAttachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[1].blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments = blendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        std::array formats = {VK_FORMAT_R32G32_UINT, VK_FORMAT_R16G16_SFLOAT};
        VkPipelineRenderingCreateInfo renderingCreateInfo {};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 2;
        renderingCreateInfo.pColorAttachmentFormats = formats.data();
        renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

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
        pipelineInfo.layout = meshPipelineLayout;

        vkCreateGraphicsPipelines(
            device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, VK_NULL_HANDLE, &meshPipeline);

        vkDestroyShaderModule(device.getDevice(), taskShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), meshShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void VisibilityPassNode::createMaskedPipeline()
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(VisibilityPushConstants);

        VkDescriptorSetLayout layouts[] = {resourceHeap.getDescriptorSetLayout(), cullPass.getObjectSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        ENGINE_VERIFY(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &maskedPipelineLayout) == VK_SUCCESS,
            "VisibilityPassNode: failed to create masked pipeline layout");

        auto vertCode = ShaderUtils::readFile("shaders/visbuffer_masked.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/visbuffer_masked.frag.spv");

        VkShaderModule vertModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule fragModule = ShaderUtils::createShaderModule(device.getDevice(), fragCode);

        VkPipelineShaderStageCreateInfo stages[2] {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth   = 1.0f;
        rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil {};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments{};
        blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[0].blendEnable = VK_FALSE;
        blendAttachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
        blendAttachments[1].blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending {};
        colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments    = blendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        std::array formats = {VK_FORMAT_R32G32_UINT, VK_FORMAT_R16G16_SFLOAT};
        VkPipelineRenderingCreateInfo renderingCreateInfo {};
        renderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount    = 2;
        renderingCreateInfo.pColorAttachmentFormats = formats.data();
        renderingCreateInfo.depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT;

        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext               = &renderingCreateInfo;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = stages;
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = maskedPipelineLayout;

        ENGINE_VERIFY(vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr, &maskedPipeline) == VK_SUCCESS,
            "VisibilityPassNode: failed to create masked pipeline");

        vkDestroyShaderModule(device.getDevice(), vertModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragModule, nullptr);
    }
}