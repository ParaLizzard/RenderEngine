#include "Renderer/Passes/SsaoPassNode.h"
#include "Core/Assert.h"
#include "Core/EngineConstants.h"
#include "Renderer/RenderSettings.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/VulkanDevice.h"

#include <array>
#include <random>

#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"
#include "Vulkan/VkUtils.h"

namespace Engine {
    SsaoPassNode::SsaoPassNode(VulkanDevice &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap):
        RenderPassNode("SSAO Pass"), device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap)
    {
        createNoiseTexture();
        createPipelines();

        ssaoDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        blurDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            ENGINE_VERIFY(descriptorPool->allocateDescriptor(ssaoSetLayout->getDescriptorSetLayout(), ssaoDescriptorSets[i]),
                "SsaoPassNode: failed to allocate SSAO descriptor sets");
            ENGINE_VERIFY(descriptorPool->allocateDescriptor(blurSetLayout->getDescriptorSetLayout(), blurDescriptorSets[i]),
                "SsaoPassNode: failed to allocate blur descriptor sets");
        }
    }

    SsaoPassNode::~SsaoPassNode()
    {
        if (noiseSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), noiseSampler, nullptr);
        if (colorSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), colorSampler, nullptr);

        if (noiseView != VK_NULL_HANDLE)
            vkDestroyImageView(device.GetHandle(), noiseView, nullptr);
        if (noiseImage != VK_NULL_HANDLE) {
            vmaDestroyImage(device.getAllocator(), noiseImage, noiseAllocation);
        }

        if (ssaoPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), ssaoPipeline, nullptr);
        if (ssaoPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.GetHandle(), ssaoPipelineLayout, nullptr);

        if (blurPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), blurPipeline, nullptr);
        if (blurPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.GetHandle(), blurPipelineLayout, nullptr);
    }

    void SsaoPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        VkExtent2D halfExtent = {currentExtent.width / 2, currentExtent.height / 2};
        renderGraph.createTransientImage("SsaoImage", VK_FORMAT_R8_UNORM, halfExtent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
        renderGraph.createTransientImage("SsaoBlurImage", VK_FORMAT_R8_UNORM, halfExtent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        renderGraph.readImage("DepthImage",
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeImage("SsaoImage",
                               VK_IMAGE_LAYOUT_GENERAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeImage("SsaoBlurImage",
                               VK_IMAGE_LAYOUT_GENERAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void SsaoPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        int i = frameInfo.frameIndex;

        VkDescriptorImageInfo depthInfo {
            colorSampler, graph.getImageView("DepthImage"), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo noiseInfo {noiseSampler, noiseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorBufferInfo bufferInfo = uboBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

        VkDescriptorImageInfo ssaoWriteInfo {
            VK_NULL_HANDLE, graph.getImageView("SsaoImage"), VK_IMAGE_LAYOUT_GENERAL};

        DescriptorWriter(*ssaoSetLayout, *descriptorPool)
            .writeImage(0, &depthInfo)
            .writeImage(1, &noiseInfo)
            .writeBuffer(2, &bufferInfo)
            .writeImage(3, &ssaoWriteInfo)
            .overwrite(ssaoDescriptorSets[i]);

        VkDescriptorImageInfo ssaoResultInfo {
            colorSampler, graph.getImageView("SsaoImage"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            
        VkDescriptorImageInfo blurWriteInfo {
            VK_NULL_HANDLE, graph.getImageView("SsaoBlurImage"), VK_IMAGE_LAYOUT_GENERAL};

        DescriptorWriter(*blurSetLayout, *descriptorPool)
            .writeImage(0, &ssaoResultInfo)
            .writeImage(1, &depthInfo)
            .writeBuffer(2, &bufferInfo)
            .writeImage(3, &blurWriteInfo)
            .overwrite(blurDescriptorSets[i]);
    }

    void SsaoPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        if (!CVarSSAOEnabled.Get()) {
            return;
        }

        int currentFrame = frameInfo.frameIndex;

        VkExtent2D halfExtent = {std::max(1u, frameInfo.extent.width / 2), std::max(1u, frameInfo.extent.height / 2)};

        SsaoUbo ubo {};
        ubo.projection = frameInfo.camera->getProjection();
        ubo.invProjection = glm::inverse(frameInfo.camera->getProjection());
        ubo.view = frameInfo.camera->getView();
        ubo.nearPlane = 0.1f;
        ubo.farPlane = 100.0f;

        memcpy(ubo.samples, ssaoKernel.data(), sizeof(ubo.samples));

        uboBuffers[currentFrame]->writeToBuffer(&ubo, sizeof(SsaoUbo), 0);
        uboBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ssaoPipeline);
        VkDescriptorSet sets[] = {ssaoDescriptorSets[currentFrame]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ssaoPipelineLayout, 0, 1, sets, 0, nullptr);

        uint32_t groupCountX = (halfExtent.width + 15) / 16;
        uint32_t groupCountY = (halfExtent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            frameInfo.renderGraph->getImage("SsaoImage"),
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        VkDependencyInfo depInfo {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                blurPipelineLayout,
                                0,
                                1,
                                &blurDescriptorSets[currentFrame],
                                0,
                                nullptr);

        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        VkImageMemoryBarrier2 restoreBarrier = VkUtils::imageBarrier(
            frameInfo.renderGraph->getImage("SsaoImage"),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        VkDependencyInfo restoreDepInfo {};
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
        for (uint32_t i = 0; i < static_cast<uint32_t>(ssaoNoise.size()); i++) {
            ssaoNoise[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
        }

        VkDeviceSize bufferSize = ssaoNoise.size() * sizeof(glm::vec4);

        Buffer stagingBuffer(device,
                             bufferSize,
                             1,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_CPU_ONLY,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             0);

        stagingBuffer.writeToBuffer(ssaoNoise.data(), bufferSize, 0);

        VkImageCreateInfo imageInfo {};
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

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(device.getAllocator(), &imageInfo, &allocInfo, &noiseImage, &noiseAllocation, nullptr);

        VkCommandBuffer cmd = device.BeginSingleTimeCommands(QueueType::Graphics);

        VkImageSubresourceRange range {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkImageMemoryBarrier2 b1 = VkUtils::imageBarrier(
            noiseImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            range
        );

        VkUtils::pipelineBarrier(cmd,
                                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                VK_ACCESS_2_NONE,
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                {b1}, {});
        device.EndSingleTimeCommands(cmd, QueueType::Graphics);

        stagingBuffer.copyBufferToImage(noiseImage, SSAO_NOISE_DIM, SSAO_NOISE_DIM, 1);

        cmd = device.BeginSingleTimeCommands(QueueType::Graphics);

        VkImageMemoryBarrier2 b2 = VkUtils::imageBarrier(
            noiseImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            range
        );

        VkUtils::pipelineBarrier(cmd,
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                VK_ACCESS_2_SHADER_READ_BIT,
                                {b2}, {});
        device.EndSingleTimeCommands(cmd, QueueType::Graphics);

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkCreateSampler(device.GetHandle(), &samplerInfo, nullptr, &noiseSampler);

        VkImageViewCreateInfo viewInfo {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = noiseImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &noiseView);
    }

    void SsaoPassNode::createPipelines()
    {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.GetHandle(), &samplerInfo, nullptr, &colorSampler);

        descriptorPool = DescriptorPool::Builder(device)
                             .setMaxSets(Constants::MAX_FRAMES_IN_FLIGHT * 2)
                             .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Constants::MAX_FRAMES_IN_FLIGHT * 2)
                             .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Constants::MAX_FRAMES_IN_FLIGHT * 4)
                             .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Constants::MAX_FRAMES_IN_FLIGHT * 2)
                             .build();

        std::default_random_engine rndEngine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
        ssaoKernel.resize(SSAO_KERNEL_SIZE);
        for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i) {
            glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
            sample = glm::normalize(sample);
            sample *= rndDist(rndEngine);
            float scale = float(i) / float(SSAO_KERNEL_SIZE);
            scale = std::lerp(0.1f, 1.0f, scale * scale);
            ssaoKernel[i] = glm::vec4(sample * scale, 0.0f);
        }

        uboBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(SsaoUbo),
                                         1,
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);
        }

        ssaoSetLayout =
            DescriptorSetLayout::Builder(device)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

        blurSetLayout = DescriptorSetLayout::Builder(device)
                            .addBinding(0,
                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        VK_SHADER_STAGE_COMPUTE_BIT) // SSAO Image
                            .addBinding(1,
                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        VK_SHADER_STAGE_COMPUTE_BIT) // Depth Image
                            .addBinding(2,
                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        VK_SHADER_STAGE_COMPUTE_BIT) // UBO
                            .addBinding(3, 
                                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 
                                        VK_SHADER_STAGE_COMPUTE_BIT) // Output Blur Image
                            .build();

        ssaoDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        blurDescriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;

        VkDescriptorSetLayout sLayout = ssaoSetLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &sLayout;
        vkCreatePipelineLayout(device.GetHandle(), &pipelineLayoutInfo, nullptr, &ssaoPipelineLayout);

        VkDescriptorSetLayout bLayout = blurSetLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &bLayout;
        vkCreatePipelineLayout(device.GetHandle(), &pipelineLayoutInfo, nullptr, &blurPipelineLayout);

        auto ssaoCompCode = ShaderUtils::readFile("shaders/ssao.comp.spv");
        auto blurCompCode = ShaderUtils::readFile("shaders/ssao_blur.comp.spv");

        VkShaderModule ssaoCompModule = ShaderUtils::createShaderModule(device.GetHandle(), ssaoCompCode);
        VkShaderModule blurCompModule = ShaderUtils::createShaderModule(device.GetHandle(), blurCompCode);

        struct SpecializationData
        {
            uint32_t kernelSize = SSAO_KERNEL_SIZE;
            float radius = SSAO_RADIUS;
        } specializationData;
        specializationData.radius = CVarSSAORadius.Get();

        std::array<VkSpecializationMapEntry, 2> specializationMapEntries = {
            VkSpecializationMapEntry(0, offsetof(SpecializationData, kernelSize), sizeof(uint32_t)),
            VkSpecializationMapEntry(1, offsetof(SpecializationData, radius), sizeof(float))};
        auto specializationInfo =
            VkSpecializationInfo(2, specializationMapEntries.data(), sizeof(specializationData), &specializationData);

        VkPipelineShaderStageCreateInfo ssaoComputeStage {};
        ssaoComputeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ssaoComputeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        ssaoComputeStage.module = ssaoCompModule;
        ssaoComputeStage.pName = "main";
        ssaoComputeStage.pSpecializationInfo = &specializationInfo;

        VkComputePipelineCreateInfo computePipelineInfo {};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = ssaoPipelineLayout;
        computePipelineInfo.stage = ssaoComputeStage;

        vkCreateComputePipelines(
            device.GetHandle(), device.getPipelineCache(), 1, &computePipelineInfo, nullptr, &ssaoPipeline);

        VkPipelineShaderStageCreateInfo blurComputeStage {};
        blurComputeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        blurComputeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        blurComputeStage.module = blurCompModule;
        blurComputeStage.pName = "main";
        
        VkComputePipelineCreateInfo blurPipelineInfo {};
        blurPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        blurPipelineInfo.layout = blurPipelineLayout;
        blurPipelineInfo.stage = blurComputeStage;

        vkCreateComputePipelines(
            device.GetHandle(), device.getPipelineCache(), 1, &blurPipelineInfo, nullptr, &blurPipeline);

        vkDestroyShaderModule(device.GetHandle(), ssaoCompModule, nullptr);
        vkDestroyShaderModule(device.GetHandle(), blurCompModule, nullptr);
    }
} // namespace Engine
