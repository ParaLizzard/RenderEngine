#include "SsaoPassNode.h"

#include <array>
#include <random>

#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    SsaoPassNode::SsaoPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap)
        : device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap)
    {
        createNoiseTexture();
        createPipelines();

        ssaoDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        blurDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (!descriptorPool->allocateDescriptor(ssaoSetLayout->getDescriptorSetLayout(), ssaoDescriptorSets[i]))
                throw std::runtime_error("SsaoPassNode: failed to allocate SSAO descriptor sets");
            if (!descriptorPool->allocateDescriptor(blurSetLayout->getDescriptorSetLayout(), blurDescriptorSets[i]))
                throw std::runtime_error("SsaoPassNode: failed to allocate blur descriptor sets");
        }
    }

    SsaoPassNode::~SsaoPassNode()
    {
        if (noiseSampler != VK_NULL_HANDLE) vkDestroySampler(device.getDevice(), noiseSampler, nullptr);
        if (colorSampler != VK_NULL_HANDLE) vkDestroySampler(device.getDevice(), colorSampler, nullptr);

        if (noiseView != VK_NULL_HANDLE) vkDestroyImageView(device.getDevice(), noiseView, nullptr);
        if (noiseImage != VK_NULL_HANDLE)
        {
            vmaDestroyImage(device.getAllocator(), noiseImage, noiseAllocation);
        }

        if (ssaoPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), ssaoPipeline, nullptr);
        if (ssaoPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(
            device.getDevice(), ssaoPipelineLayout, nullptr);

        if (blurPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), blurPipeline, nullptr);
        if (blurPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(
            device.getDevice(), blurPipelineLayout, nullptr);
    }

    void SsaoPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        VkExtent2D halfExtent = {currentExtent.width / 2, currentExtent.height / 2};
        renderGraph.createTransientImage("SsaoImage", VK_FORMAT_R8_UNORM, halfExtent);
        renderGraph.createTransientImage("SsaoBlurImage", VK_FORMAT_R8_UNORM, halfExtent);

        renderGraph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("G_NormalRoughness", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeImage("SsaoImage", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        renderGraph.writeImage("SsaoBlurImage", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }

    void SsaoPassNode::resolve(const RenderGraph& graph, const FrameInfo& frameInfo)
    {
        int i = frameInfo.frameIndex;

        VkDescriptorImageInfo depthInfo{
            colorSampler, graph.getImageView("DepthImage"), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo normalInfo{
            colorSampler, graph.getImageView("G_NormalRoughness"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo noiseInfo{noiseSampler, noiseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorBufferInfo bufferInfo = uboBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

        LveDescriptorWriter(*ssaoSetLayout, *descriptorPool)
            .writeImage(0, &depthInfo)
            .writeImage(1, &normalInfo)
            .writeImage(2, &noiseInfo)
            .writeBuffer(3, &bufferInfo)
            .overwrite(ssaoDescriptorSets[i]);

        VkDescriptorImageInfo ssaoResultInfo{
            colorSampler, graph.getImageView("SsaoImage"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        LveDescriptorWriter(*blurSetLayout, *descriptorPool)
            .writeImage(0, &ssaoResultInfo)
            .overwrite(blurDescriptorSets[i]);
    }

    void SsaoPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        int currentFrame = frameInfo.frameIndex;

        VkExtent2D halfExtent = {
            std::max(1u, frameInfo.extent.width / 2),
            std::max(1u, frameInfo.extent.height / 2)
        };

        SsaoUbo ubo{};
        ubo.projection = frameInfo.camera->getProjection();
        ubo.invProjection = glm::inverse(frameInfo.camera->getProjection());
        ubo.view = frameInfo.camera->getView();

        memcpy(ubo.samples, ssaoKernel.data(), sizeof(ubo.samples));

        uboBuffers[currentFrame]->writeToBuffer(&ubo, sizeof(SsaoUbo), 0);
        uboBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

        VkRenderingAttachmentInfo ssaoAttachment{};
        ssaoAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        ssaoAttachment.imageView = frameInfo.renderGraph->getImageView("SsaoImage");
        ssaoAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ssaoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ssaoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ssaoAttachment.clearValue.color = {{1.0f, 1.0f, 1.0f, 1.0f}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = halfExtent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &ssaoAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(halfExtent.width);
        viewport.height = static_cast<float>(halfExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = halfExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline);
        VkDescriptorSet sets[] = {ssaoDescriptorSets[currentFrame]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipelineLayout, 0, 1, sets, 0, nullptr);

        vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle
        vkCmdEndRendering(cmd);

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image = frameInfo.renderGraph->getImage("SsaoImage");
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        VkRenderingAttachmentInfo blurAttachment{};
        blurAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        blurAttachment.imageView = frameInfo.renderGraph->getImageView("SsaoBlurImage");
        blurAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        blurAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        blurAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo blurRenderInfo{};
        blurRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        blurRenderInfo.renderArea.offset = {0, 0};
        blurRenderInfo.renderArea.extent = halfExtent;
        blurRenderInfo.layerCount = 1;
        blurRenderInfo.colorAttachmentCount = 1;
        blurRenderInfo.pColorAttachments = &blurAttachment;

        vkCmdBeginRendering(cmd, &blurRenderInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipelineLayout, 0, 1,
                                &blurDescriptorSets[currentFrame], 0, nullptr);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        VkImageMemoryBarrier2 restoreBarrier{};
        restoreBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        restoreBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        restoreBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        restoreBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        restoreBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        restoreBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        restoreBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        restoreBarrier.image = frameInfo.renderGraph->getImage("SsaoImage");
        restoreBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo restoreDepInfo{};
        restoreDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        restoreDepInfo.imageMemoryBarrierCount = 1;
        restoreDepInfo.pImageMemoryBarriers = &restoreBarrier;
        vkCmdPipelineBarrier2(cmd, &restoreDepInfo);
    }

    void SsaoPassNode::createNoiseTexture()
    {
        std::default_random_engine rndEngine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

        std::vector<glm::vec4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
        for (uint32_t i = 0; i < static_cast<uint32_t>(ssaoNoise.size()); i++)
        {
            ssaoNoise[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
        }

        VkDeviceSize bufferSize = ssaoNoise.size() * sizeof(glm::vec4);

        Buffer stagingBuffer(device, bufferSize, 1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_CPU_ONLY,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0);

        stagingBuffer.writeToBuffer(ssaoNoise.data(), bufferSize, 0);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {SSAO_NOISE_DIM, SSAO_NOISE_DIM, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(device.getAllocator(), &imageInfo, &allocInfo, &noiseImage, &noiseAllocation, nullptr);

        VkCommandBuffer cmd = device.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = noiseImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
        device.endSingleTimeCommands(cmd);

        stagingBuffer.copyBufferToImage(noiseImage, SSAO_NOISE_DIM, SSAO_NOISE_DIM, 1);

        cmd = device.beginSingleTimeCommands();
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
        device.endSingleTimeCommands(cmd);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &noiseSampler);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = noiseImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &noiseView);
    }

    void SsaoPassNode::createPipelines()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &colorSampler);

        descriptorPool = LveDescriptorPool::Builder(device)
                         .setMaxSets(Renderer::MAX_FRAMES_IN_FLIGHT * 2)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Renderer::MAX_FRAMES_IN_FLIGHT * 4)
                         .build();

        std::default_random_engine rndEngine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
        ssaoKernel.resize(SSAO_KERNEL_SIZE);
        for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
        {
            glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
            sample = glm::normalize(sample);
            sample *= rndDist(rndEngine);
            float scale = float(i) / float(SSAO_KERNEL_SIZE);
            scale = std::lerp(0.1f, 1.0f, scale * scale);
            ssaoKernel[i] = glm::vec4(sample * scale, 0.0f);
        }

        uboBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++)
        {
            uboBuffers[i] = std::make_unique<Buffer>(
                device, sizeof(SsaoUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0);
        }

        ssaoSetLayout = LveDescriptorSetLayout::Builder(device)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Depth
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT) // Normal
                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Noise
                        .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        // Kernel + Matrices
                        .build();

        blurSetLayout = LveDescriptorSetLayout::Builder(device)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT) // SSAO Image
                        .build();

        ssaoDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        blurDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;

        VkDescriptorSetLayout sLayout = ssaoSetLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &sLayout;
        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &ssaoPipelineLayout);

        VkDescriptorSetLayout bLayout = blurSetLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &bLayout;
        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &blurPipelineLayout);

        auto vertCode = ShaderUtils::readFile("shaders/fullscreen.vert.spv");
        auto ssaoFragCode = ShaderUtils::readFile("shaders/ssao.frag.spv");
        auto blurFragCode = ShaderUtils::readFile("shaders/ssao_blur.frag.spv");

        VkShaderModule vertModule = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule ssaoFragModule = ShaderUtils::createShaderModule(device.getDevice(), ssaoFragCode);
        VkShaderModule blurFragModule = ShaderUtils::createShaderModule(device.getDevice(), blurFragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
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
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = 0xF;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = VK_FORMAT_R8_UNORM;
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

        struct SpecializationData
        {
            uint32_t kernelSize = SSAO_KERNEL_SIZE;
            float radius = SSAO_RADIUS;
        } specializationData;

        std::array<VkSpecializationMapEntry, 2> specializationMapEntries = {
            VkSpecializationMapEntry(0, offsetof(SpecializationData, kernelSize), sizeof(uint32_t)),
            VkSpecializationMapEntry(1, offsetof(SpecializationData, radius), sizeof(float))
        };
        auto specializationInfo = VkSpecializationInfo(2, specializationMapEntries.data(), sizeof(specializationData),
                                                       &specializationData);

        shaderStages[1].module = ssaoFragModule;
        shaderStages[1].pSpecializationInfo = &specializationInfo;
        pipelineInfo.layout = ssaoPipelineLayout;
        vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr,
                                  &ssaoPipeline);

        shaderStages[1].module = blurFragModule;
        shaderStages[1].pSpecializationInfo = nullptr;
        pipelineInfo.layout = blurPipelineLayout;
        vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr,
                                  &blurPipeline);

        vkDestroyShaderModule(device.getDevice(), vertModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), ssaoFragModule, nullptr);
        vkDestroyShaderModule(device.getDevice(), blurFragModule, nullptr);
    }
}
