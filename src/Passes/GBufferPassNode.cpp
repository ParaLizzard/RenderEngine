#include "GBufferPassNode.h"

namespace Engine
{
    GBufferPassNode::GBufferPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap, LightPassNode& prepassNode)
        : device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap), prepassNode(prepassNode)
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &ssaoSampler);

        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &linearSampler);

        ssaoSetLayout = LveDescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Albedo
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Normal
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Depth
            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // SSAO
            .build();

        descriptorPool = LveDescriptorPool::Builder(device)
            .setMaxSets(Renderer::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Renderer::MAX_FRAMES_IN_FLIGHT * 4)
            .build();

        ssaoDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
            if (!descriptorPool->allocateDescriptor(ssaoSetLayout->getDescriptorSetLayout(), ssaoDescriptorSets[i]))
                throw std::runtime_error("ForwardPassNode: failed to allocate GBuffer descriptor sets");
        }

        createPipelineLayout();
        createPipeline();
    }

    GBufferPassNode::~GBufferPassNode()
    {
        if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
        if (ssaoSampler != VK_NULL_HANDLE) vkDestroySampler(device.getDevice(), ssaoSampler, nullptr);
        if (linearSampler != VK_NULL_HANDLE) vkDestroySampler(device.getDevice(), linearSampler, nullptr);
    }

    void GBufferPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        VkFormat currentFormat = renderer.getSwapChain().getSwapChainImageFormat();

        renderGraph.createTransientImage("SceneColorImage", currentFormat, currentExtent);

        renderGraph.readImage("G_AlbedoMetallic", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("G_NormalRoughness", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("SsaoBlurImage", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeImage("SceneColorImage", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }

    void GBufferPassNode::resolve(const RenderGraph& graph, const FrameInfo& frameInfo)
    {
        VkDescriptorImageInfo albedoInfo{ssaoSampler, graph.getImageView("G_AlbedoMetallic"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo normalInfo{ssaoSampler, graph.getImageView("G_NormalRoughness"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo depthInfo{ssaoSampler, graph.getImageView("DepthImage"), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo ssaoInfo{linearSampler, graph.getImageView("SsaoBlurImage"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        LveDescriptorWriter(*ssaoSetLayout, *descriptorPool)
            .writeImage(0, &albedoInfo)
            .writeImage(1, &normalInfo)
            .writeImage(2, &depthInfo)
            .writeImage(3, &ssaoInfo)
            .overwrite(ssaoDescriptorSets[frameInfo.frameIndex]);
    }

    void GBufferPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ForwardPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkDescriptorSetLayout layouts[] = {
            resourceHeap.getDescriptorSetLayout(),      // Set 0
            ssaoSetLayout->getDescriptorSetLayout()     // Set 1
        };
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout");
    }

    void GBufferPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/fullscreen.vert.spv");
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

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = renderer.getSwapChain().getSwapChainImageFormat();
        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &colorFormat;

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

        if (vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
            throw std::runtime_error("failed to create deferred graphics pipeline");

        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    }

    void GBufferPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        glm::mat4 projection = frameInfo.camera.getProjection();
        glm::mat4 view = frameInfo.camera.getView();
        glm::mat4 flipMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 viewProjection = projection * view * flipMatrix;

        glm::mat4 invViewProj = glm::inverse(viewProjection);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = frameInfo.renderGraph->getImageView("SceneColorImage");
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.02f, 0.02f, 0.02f, 1.0f}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = frameInfo.extent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkDescriptorSet sets[] = { resourceHeap.getDescriptorSet(), ssaoDescriptorSets[currentFrame] };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

        ForwardPushConstants ps{};
        ps.viewProjection = invViewProj;
        ps.ssao = frameInfo.enableSSAO;
        ps.debugIsTransparent = false;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ForwardPushConstants), &ps);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);
    }
}