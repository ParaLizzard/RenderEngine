
#include "HiZPassNode.h"

#include "Renderer/RenderGraph.h"
#include "Vulkan/Device.h"
#include "Renderer/Renderer.h"

#define A_CPU
#include <ffx_a.h>
#include <ffx_spd.h>

#include "Renderer/ShaderUtils.h"

namespace Engine
{
    HiZPassNode::HiZPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap):
    RenderPassNode("Hi-Z Pass"),
    device(device),
    renderer(renderer),
    megaBuffer(megaBuffer),
    resourceHeap(resourceHeap)
    {
        VkSamplerCreateInfo samplerInfo2 {};
        samplerInfo2.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo2.magFilter = VK_FILTER_NEAREST;
        samplerInfo2.minFilter = VK_FILTER_NEAREST;
        samplerInfo2.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo2.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo2.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &samplerInfo2, nullptr, &nearestSampler);

        atomicCounterBuffer = std::make_unique<Buffer>(
            device,
            sizeof(uint32_t) * 6,
            1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            1
        );

        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        createHiZResources(currentExtent);

        descriptorPool = DescriptorPool::Builder(device)
                        .setMaxSets(Config::MAX_FRAMES_IN_FLIGHT)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Config::MAX_FRAMES_IN_FLIGHT * 1)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Config::MAX_FRAMES_IN_FLIGHT * 12)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Config::MAX_FRAMES_IN_FLIGHT * 1)
                        .build();

        setLayout = DescriptorSetLayout::Builder(device)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 12)
                .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
                .build();

        descriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        createPipelineLayout();
        createPipeline();
    }

    HiZPassNode::~HiZPassNode()
    {
        destroyHiZResources();

        if (nearestSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.getDevice(), nearestSampler, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void HiZPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        VkExtent2D newHiZExtent = { std::bit_ceil(currentExtent.width), std::bit_ceil(currentExtent.height) };
        if (hizExtent.width != newHiZExtent.width || hizExtent.height != newHiZExtent.height || HiZImage == VK_NULL_HANDLE) {
            destroyHiZResources();
            createHiZResources(currentExtent);
        }

        renderGraph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.writeImage("HiZImage", VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void HiZPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        uint32_t currentFrame = frameInfo.frameIndex;

        if (descriptorSets[currentFrame] == VK_NULL_HANDLE) {
            descriptorPool->allocateDescriptor(setLayout->getDescriptorSetLayout(), descriptorSets[currentFrame]);
        }

        VkDescriptorImageInfo srcDepthInfo = {
            nearestSampler,
            graph.getImageView("DepthImage"),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };

        VkDescriptorImageInfo dstInfos[12];
        for (uint32_t i = 0; i < 12; i++) {
            uint32_t viewIdx = std::min(i, static_cast<uint32_t>(HizMipViews.empty() ? 0 : HizMipViews.size() - 1));
            dstInfos[i] = {
                VK_NULL_HANDLE,
                HizMipViews.empty() ? VK_NULL_HANDLE : HizMipViews[viewIdx],
                VK_IMAGE_LAYOUT_GENERAL
            };
        }

        VkDescriptorBufferInfo counterBufferInfo = atomicCounterBuffer->descriptorInfo(sizeof(uint32_t) * 6, 0);

        DescriptorWriter(*setLayout, *descriptorPool)
            .writeImage(0, &srcDepthInfo)
            .writeImageArray(1, dstInfos, 12)
            .writeBuffer(2, &counterBufferInfo)
            .overwrite(descriptorSets[currentFrame]);
    }

    void HiZPassNode::registerResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::registerResources(graph, frameInfo);

        graph.registerPhysicalImage(
            "HiZImage",
            HiZImage,
            HiZImageView,
            VK_FORMAT_R32_SFLOAT,
            hizExtent,
            VK_IMAGE_LAYOUT_UNDEFINED,
            1,
            hizMipLevels
        );
    }

    void HiZPassNode::updateResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::updateResources(graph, frameInfo);

        graph.updateImageHandle("HiZImage", HiZImage, HiZImageView, hizExtent, hizMipLevels);
    }

    void HiZPassNode::execute(VkCommandBuffer&cmd, FrameInfo &frameInfo)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        VkDescriptorSet sets[] = {descriptorSets[frameInfo.frameIndex]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, sets, 0, nullptr);

        VkExtent2D depthExtent = renderer.getSwapChain().getSwapChainExtent();
        VkExtent2D hizExtent   = getHiZExtent(depthExtent);
        
        AU1 dispatchThreadGroupCountXY[2];
        AU1 workGroupOffset[2];
        AU1 numWorkGroupsAndMips[2];
        AU1 rectInfo[4] = { 0, 0, depthExtent.width, depthExtent.height };

        SpdSetup(
         dispatchThreadGroupCountXY,
         workGroupOffset,
         numWorkGroupsAndMips,
         rectInfo,
         -1
        );

        HiZPushConstants pushConstants {};
        pushConstants.mips = numWorkGroupsAndMips[1];
        pushConstants.numWorkGroups = numWorkGroupsAndMips[0];
        pushConstants.workGroupOffset[0] = workGroupOffset[0];
        pushConstants.workGroupOffset[1] = workGroupOffset[1];
        
        vkCmdPushConstants(
            cmd,
            pipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(HiZPushConstants),
            &pushConstants
        );

        vkCmdDispatch(cmd, dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 1);
    }

    void HiZPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(HiZPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;

        VkDescriptorSetLayout sLayout = setLayout->getDescriptorSetLayout();
        pipelineLayoutInfo.pSetLayouts = &sLayout;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("HiZ: failed to create pipeline layout");
        }
    }

    void HiZPassNode::createPipeline()
    {
        auto CompCode = ShaderUtils::readFile("shaders/hiz_generate.comp.spv");

        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), CompCode);

        VkPipelineShaderStageCreateInfo computeStage {};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStage.module = compModule;
        computeStage.pName = "main";

        VkComputePipelineCreateInfo computePipelineInfo {};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = pipelineLayout;
        computePipelineInfo.stage = computeStage;

        vkCreateComputePipelines(
            device.getDevice(), device.getPipelineCache(), 1, &computePipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }

    void HiZPassNode::createHiZResources(VkExtent2D depthExtent)
    {
        hizExtent = { std::bit_ceil(depthExtent.width), std::bit_ceil(depthExtent.height) };
        hizMipLevels = std::bit_width(std::max(hizExtent.width, hizExtent.height));

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R32_SFLOAT;
        imageInfo.extent = {hizExtent.width, hizExtent.height, 1};
        imageInfo.mipLevels = hizMipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        device.createImageWithInfo(imageInfo, HiZImage, HiZMemory);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = HiZImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = hizMipLevels;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;

        vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &HiZImageView);

        HizMipViews.resize(hizMipLevels);
        for (uint32_t i = 0; i < hizMipLevels; i++) {
            viewInfo.subresourceRange.baseMipLevel = i;
            viewInfo.subresourceRange.levelCount = 1;
            vkCreateImageView(device.getDevice(), &viewInfo, nullptr, &HizMipViews[i]);
        }

    }

    void HiZPassNode::destroyHiZResources()
    {
        for (auto &view : HizMipViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device.getDevice(), view, nullptr);
                view = VK_NULL_HANDLE;
            }
        }
        HizMipViews.clear();

        if (HiZImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device.getDevice(), HiZImageView, nullptr);
            HiZImageView = VK_NULL_HANDLE;
        }

        if (HiZImage != VK_NULL_HANDLE) {
            vmaDestroyImage(device.getAllocator(), HiZImage, HiZMemory);
            HiZImage = VK_NULL_HANDLE;
            HiZMemory = VK_NULL_HANDLE;
        }
    }


} // Engine