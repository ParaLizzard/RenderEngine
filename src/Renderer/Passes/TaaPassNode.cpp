//
// Created by Martin Varga on 11.08.2026.
//

#include "TaaPassNode.h"
#include "Core/Assert.h"
#include "Core/EngineConstants.h"
#include "Renderer/RenderSettings.h"

#include "Renderer/Renderer.h"
#include "Renderer/ShaderUtils.h"

namespace Engine {
    TaaPassNode::TaaPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap) :
    RenderPassNode("TAA Pass"),
    device(device),
    renderer(renderer),
    megabBuffer(megaBuffer),
    resourceHeap(resourceHeap)
    {
        extent = renderer.getSwapChain().getSwapChainExtent();
        createHistoryResources();

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &linearSampler);

        VkSamplerCreateInfo samplerInfo2 {};
        samplerInfo2.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo2.magFilter = VK_FILTER_NEAREST;
        samplerInfo2.minFilter = VK_FILTER_NEAREST;
        samplerInfo2.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo2.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo2.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &samplerInfo2, nullptr, &nearestSampler);

        descriptorPool = DescriptorPool::Builder(device)
                         .setMaxSets(Constants::MAX_FRAMES_IN_FLIGHT)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Constants::MAX_FRAMES_IN_FLIGHT * 5)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Constants::MAX_FRAMES_IN_FLIGHT * 2)
                         .build();

        setLayout =
            DescriptorSetLayout::Builder(device)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                .addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                .build();

        descriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        createPipelineLayout();
        createPipeline();
    }

    TaaPassNode::~TaaPassNode()
    {
        destroyHistoryResources();

        if (linearSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.getDevice(), linearSampler, nullptr);
        if (nearestSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.getDevice(), nearestSampler, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void TaaPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        if (extent.width != currentExtent.width || extent.height != currentExtent.height) {
            extent = currentExtent;
            destroyHistoryResources();
            createHistoryResources();
        }

        renderGraph.readImage("FinalRender", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("VelocityBuffer", VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
        renderGraph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.readImage("TaaHistoryRead", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.writeImage("TaaHistoryWrite", VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

        renderGraph.readImage("TaaPrevVelocity", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.writeImage("TaaVelocityHistoryWrite", VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        renderGraph.createTransientImage("TaaOutput", VK_FORMAT_R16G16B16A16_SFLOAT, extent, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        renderGraph.writeImage("TaaOutput", VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void TaaPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        uint32_t currentFrame = frameInfo.frameIndex;

        if (descriptorSets[currentFrame] == VK_NULL_HANDLE) {
            descriptorPool->allocateDescriptor(setLayout->getDescriptorSetLayout(), descriptorSets[currentFrame]);
        }

        VkDescriptorImageInfo imageInfo1 = {linearSampler, graph.getImageView("FinalRender"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo imageInfo2 = {nearestSampler, graph.getImageView("VelocityBuffer"), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo imageInfo3 = {nearestSampler, graph.getImageView("DepthImage"), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo imageInfo4 = {linearSampler, graph.getImageView("TaaHistoryRead"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo imageInfo5 = {VK_NULL_HANDLE, graph.getImageView("TaaHistoryWrite"), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo imageInfo6 = {VK_NULL_HANDLE, graph.getImageView("TaaOutput"), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo imageInfo7 = {nearestSampler, graph.getImageView("TaaPrevVelocity"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        DescriptorWriter(*setLayout, *descriptorPool)
             .writeImage(0, &imageInfo1)
             .writeImage(1, &imageInfo2)
             .writeImage(2, &imageInfo3)
             .writeImage(3, &imageInfo4)
             .writeImage(4, &imageInfo5)
             .writeImage(5, &imageInfo6)
             .writeImage(6, &imageInfo7)
             .overwrite(descriptorSets[currentFrame]);
    }

    void TaaPassNode::execute(VkCommandBuffer&cmd, FrameInfo &frameInfo)
    {
        if (CVarAAMethod.Get() != 2) {
            return;
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        VkDescriptorSet sets[] = {descriptorSets[frameInfo.frameIndex]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, sets, 0, nullptr);

        uint32_t groupCountX = (extent.width + 15) / 16;
        uint32_t groupCountY = (extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        uint32_t currIdx = 1 - historyPingPong;
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {extent.width, extent.height, 1};
        vkCmdCopyImage(cmd,
                       frameInfo.renderGraph->getImage("VelocityBuffer"),
                       VK_IMAGE_LAYOUT_GENERAL,
                       velocityHistoryBuffers[currIdx].image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &copyRegion);

        historyPingPong = 1 - historyPingPong;
    }

    void TaaPassNode::registerResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::registerResources(graph, frameInfo);

        uint32_t prevIdx = historyPingPong;
        uint32_t currIdx = 1 - historyPingPong;

        graph.registerPhysicalImage("TaaHistoryRead", historyBuffers[prevIdx].image, historyBuffers[prevIdx].view, VK_FORMAT_R16G16B16A16_SFLOAT, extent, VK_IMAGE_LAYOUT_UNDEFINED);
        graph.registerPhysicalImage("TaaHistoryWrite", historyBuffers[currIdx].image, historyBuffers[currIdx].view, VK_FORMAT_R16G16B16A16_SFLOAT, extent, VK_IMAGE_LAYOUT_UNDEFINED);
        graph.registerPhysicalImage("TaaPrevVelocity", velocityHistoryBuffers[prevIdx].image, velocityHistoryBuffers[prevIdx].view, VK_FORMAT_R16G16_SFLOAT, extent, VK_IMAGE_LAYOUT_UNDEFINED);
        graph.registerPhysicalImage("TaaVelocityHistoryWrite", velocityHistoryBuffers[currIdx].image, velocityHistoryBuffers[currIdx].view, VK_FORMAT_R16G16_SFLOAT, extent, VK_IMAGE_LAYOUT_UNDEFINED);
    }

    void TaaPassNode::updateResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::updateResources(graph, frameInfo);

        uint32_t prevIdx = historyPingPong;
        uint32_t currIdx = 1 - historyPingPong;

        graph.updateImageHandle("TaaHistoryRead", historyBuffers[prevIdx].image, historyBuffers[prevIdx].view, extent);
        graph.updateImageHandle("TaaHistoryWrite", historyBuffers[currIdx].image, historyBuffers[currIdx].view, extent);
        graph.updateImageHandle("TaaPrevVelocity", velocityHistoryBuffers[prevIdx].image, velocityHistoryBuffers[prevIdx].view, extent);
        graph.updateImageHandle("TaaVelocityHistoryWrite", velocityHistoryBuffers[currIdx].image, velocityHistoryBuffers[currIdx].view, extent);
    }

    void TaaPassNode::createPipelineLayout()
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;

        VkDescriptorSetLayout sLayout = setLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &sLayout;

        ENGINE_VERIFY(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS,
            "TAA: failed to create pipeline layout");
    }

    void TaaPassNode::createPipeline()
    {
        auto CompCode = ShaderUtils::readFile("shaders/taa.comp.spv");

        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), CompCode);

        struct SpecializationData
        {
           float modulationFactor = Constants::TAA_MODULATION_FACTOR;
        } specializationData;

        VkSpecializationMapEntry entry = {0, offsetof(SpecializationData, modulationFactor), sizeof(float)};

        VkSpecializationInfo specializationInfo = {1,&entry, sizeof(SpecializationData), &specializationData};

        VkPipelineShaderStageCreateInfo computeStage {};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = compModule;
        computeStage.pName = "main";
        computeStage.pSpecializationInfo = &specializationInfo;

        VkComputePipelineCreateInfo computePipelineInfo {};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = pipelineLayout;
        computePipelineInfo.stage = computeStage;

        vkCreateComputePipelines(
            device.getDevice(), device.getPipelineCache(), 1, &computePipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }

    void TaaPassNode::createHistoryResources()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        for(uint32_t i = 0; i < 2; i++) {
            device.createImageWithInfo(imageInfo, historyBuffers[i].image, historyBuffers[i].allocation);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = historyBuffers[i].image;
            viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.pNext = nullptr;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;

            vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &historyBuffers[i].view);
        }

        VkImageCreateInfo velocityImageInfo{};
        velocityImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        velocityImageInfo.imageType = VK_IMAGE_TYPE_2D;
        velocityImageInfo.format = VK_FORMAT_R16G16_SFLOAT;
        velocityImageInfo.extent = {extent.width, extent.height, 1};
        velocityImageInfo.mipLevels = 1;
        velocityImageInfo.arrayLayers = 1;
        velocityImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        velocityImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        velocityImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        for (uint32_t i = 0; i < 2; i++) {
            device.createImageWithInfo(velocityImageInfo, velocityHistoryBuffers[i].image, velocityHistoryBuffers[i].allocation);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = velocityHistoryBuffers[i].image;
            viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.pNext = nullptr;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;

            vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &velocityHistoryBuffers[i].view);
        }
    }

    void TaaPassNode::destroyHistoryResources()
    {
        for(uint32_t i = 0; i < 2; i++) {
            if (historyBuffers[i].view != VK_NULL_HANDLE) {
                vkDestroyImageView(device.getDevice(), historyBuffers[i].view, nullptr);
                historyBuffers[i].view = VK_NULL_HANDLE;
            }
            if (historyBuffers[i].image != VK_NULL_HANDLE) {
                vmaDestroyImage(device.getAllocator(), historyBuffers[i].image, historyBuffers[i].allocation);
                historyBuffers[i].image = VK_NULL_HANDLE;
            }
            if (velocityHistoryBuffers[i].view != VK_NULL_HANDLE) {
                vkDestroyImageView(device.getDevice(), velocityHistoryBuffers[i].view, nullptr);
                velocityHistoryBuffers[i].view = VK_NULL_HANDLE;
            }
            if (velocityHistoryBuffers[i].image != VK_NULL_HANDLE) {
                vmaDestroyImage(device.getAllocator(), velocityHistoryBuffers[i].image, velocityHistoryBuffers[i].allocation);
                velocityHistoryBuffers[i].image = VK_NULL_HANDLE;
            }
        }
    }
} // namespace Engine
