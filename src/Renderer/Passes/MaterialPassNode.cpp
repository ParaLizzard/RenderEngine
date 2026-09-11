#include "Renderer/Passes/MaterialPassNode.h"
#include "Core/Assert.h"
#include "Core/EngineConstants.h"
#include "Renderer/RenderSettings.h"
#include <array>

#include "Vulkan/Buffer.h"
#include "Vulkan/Descriptor.h"
#include "Vulkan/VulkanDevice.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include "Renderer/ShaderUtils.h"
#include "Renderer/Passes/CullPassNode.h"

namespace Engine {
    MaterialPassNode::MaterialPassNode(VulkanDevice &device,
                     Renderer &renderer,
                     Model &megaBuffer,
                     ResourceHeap &resourceHeap,
                     CullPassNode &cullPass,
                     RenderGraph &renderGraph):
        RenderPassNode("Material Pass"), device(device), megaBuffer(megaBuffer), renderer(renderer), resourceHeap(resourceHeap), cullPass(cullPass), renderGraph(renderGraph)
    {
        globalPool = DescriptorPool::Builder(device)
                     .setMaxSets(Constants::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Constants::MAX_FRAMES_IN_FLIGHT * 4)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Constants::MAX_FRAMES_IN_FLIGHT)
                     .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
                     .build();

        globalSetLayout = DescriptorSetLayout::Builder(device)
                          .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .build();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.maxAnisotropy = device.getMaxAnisotropy();
        samplerInfo.pNext = VK_NULL_HANDLE;
        ENGINE_VERIFY(vkCreateSampler(device.GetHandle(), &samplerInfo, nullptr, &sampler) == VK_SUCCESS,
            "MaterialPassNode: Failed to create texture sampler");

        VkSamplerCreateInfo nearestSamplerInfo{};
        nearestSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        nearestSamplerInfo.magFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.minFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        nearestSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.anisotropyEnable = VK_FALSE;
        nearestSamplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        nearestSamplerInfo.maxAnisotropy = 1.0f;
        nearestSamplerInfo.pNext = VK_NULL_HANDLE;
        ENGINE_VERIFY(vkCreateSampler(device.GetHandle(), &nearestSamplerInfo, nullptr, &nearestSampler) == VK_SUCCESS,
            "MaterialPassNode: Failed to create nearest texture sampler");

        VkSamplerCreateInfo shadowSamplerInfo{};
        shadowSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        shadowSamplerInfo.maxAnisotropy = 1.0f;
        shadowSamplerInfo.pNext = VK_NULL_HANDLE;
        ENGINE_VERIFY(vkCreateSampler(device.GetHandle(), &shadowSamplerInfo, nullptr, &shadowSampler) == VK_SUCCESS,
            "MaterialPassNode: Failed to create shadow sampler");

        shadowSamplerInfo.compareEnable = VK_TRUE;
        shadowSamplerInfo.compareOp = VK_COMPARE_OP_LESS;
        ENGINE_VERIFY(vkCreateSampler(device.GetHandle(), &shadowSamplerInfo, nullptr, &hardwareShadowSampler) == VK_SUCCESS,
            "MaterialPassNode: Failed to create hardware shadow sampler");

        descriptorSets.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            ENGINE_VERIFY(globalPool->allocateDescriptor(globalSetLayout->getDescriptorSetLayout(), descriptorSets[i]),
                "MaterialPassNode: Failed to allocate descriptor sets!");
        }

        createPipelineLayout();
        createPipeline();
    }

    MaterialPassNode::~MaterialPassNode()
    {
        if (nearestSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), nearestSampler, nullptr);
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), sampler, nullptr);
        if (shadowSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), shadowSampler, nullptr);
        if (hardwareShadowSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.GetHandle(), hardwareShadowSampler, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.GetHandle(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.GetHandle(), pipelineLayout, nullptr);
    }

    void MaterialPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        renderGraph.readImage("VisBuffer",
                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("CsmImage",
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("SsaoBlurImage",
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);

        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        renderGraph.createTransientImage("FinalRender",
                                         VK_FORMAT_R16G16B16A16_SFLOAT,
                                         currentExtent,
                                         1,
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        renderGraph.writeImage("FinalRender",
                               VK_IMAGE_LAYOUT_GENERAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void MaterialPassNode::registerResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        uint32_t currentFrame = frameInfo.frameIndex;

        graph.registerPhysicalBuffer("MaterialSSBO",
                                     resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                     resourceHeap.getMaterialBufferInfo(currentFrame).range,
                                     VK_PIPELINE_STAGE_2_HOST_BIT,
                                     VK_ACCESS_2_HOST_WRITE_BIT);
    }

    void MaterialPassNode::updateResources(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        uint32_t currentFrame = frameInfo.frameIndex;

        graph.updateBufferHandle("MaterialSSBO",
                                 resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                 resourceHeap.getMaterialBufferSize());
    }

    void MaterialPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();
        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
        VkDescriptorSet sets[] = {bindlessSet, descriptorSets[currentFrame]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);

        glm::mat4 view = frameInfo.camera->getView();
        glm::mat4 viewProjection = frameInfo.curViewProj;

        static uint32_t currentDebugMode = 0;

        if (frameInfo.input) {
            if (frameInfo.input->IsKeyJustPressed(KeyCode::F1)) {
                currentDebugMode = (currentDebugMode == 1) ? 0 : 1;
            }
            if (frameInfo.input->IsKeyJustPressed(KeyCode::F2)) {
                currentDebugMode = (currentDebugMode == 2) ? 0 : 2;
            }
            if (frameInfo.input->IsKeyJustPressed(KeyCode::F3)) {
                currentDebugMode = (currentDebugMode == 3) ? 0 : 3;
            }
            if (frameInfo.input->IsKeyJustPressed(KeyCode::F6)) {
                currentDebugMode = (currentDebugMode == 4) ? 0 : 4;
            }
        }

        MaterialPushConstants pc{};
        pc.viewProj = viewProjection;
        pc.view = view;
        pc.cameraPos = frameInfo.camera->getPosition();
        pc.enableSSAO = (CVarSSAOEnabled.Get() && frameInfo.enableSSAO) ? 1 : 0;
        pc.debugMode = currentDebugMode;
        pc.ssaoStrength = CVarSSAOStrength.Get();
        pc.jitterOffset = frameInfo.subpixelJitter;
        pc.resolution = {static_cast<float>(extent.width), static_cast<float>(extent.height)};
        pc.rcpResolution = {1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height)};

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MaterialPushConstants), &pc);

        uint32_t groupCountX = (extent.width + 15) / 16;
        uint32_t groupCountY = (extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    void MaterialPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        uint32_t currentFrame = frameInfo.frameIndex;

        RenderPassNode::resolve(graph, frameInfo);

        VkDescriptorImageInfo visBufferInfo{};
        visBufferInfo.imageView = graph.getImageView("VisBuffer");
        visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        visBufferInfo.sampler = nearestSampler;

        VkDescriptorImageInfo finalRenderInfo{};
        finalRenderInfo.imageView = graph.getImageView("FinalRender");
        finalRenderInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo ssaoInfo{};
        ssaoInfo.imageView = graph.getImageView("SsaoBlurImage");
        ssaoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ssaoInfo.sampler = sampler;

        VkDescriptorImageInfo csmInfo{};
        csmInfo.imageView = graph.getImageView("CsmImage");
        csmInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        csmInfo.sampler = shadowSampler;

        VkDescriptorImageInfo csmHardwareInfo{};
        csmHardwareInfo.imageView = graph.getImageView("CsmImage");
        csmHardwareInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        csmHardwareInfo.sampler = hardwareShadowSampler;

        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeImage(3, &visBufferInfo)
            .writeImage(9, &finalRenderInfo)
            .writeImage(10, &ssaoInfo)
            .writeImage(12, &csmInfo)
            .writeImage(13, &csmHardwareInfo)
            .overwrite(descriptorSets[currentFrame]);
    }

    void MaterialPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MaterialPushConstants);

        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();
        VkDescriptorSetLayout layouts[] = {bindlessLayout, globalSetLayout->getDescriptorSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        ENGINE_VERIFY(vkCreatePipelineLayout(device.GetHandle(), &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS,
            "MaterialPassNode: Failed to create compute pipeline layout");
    }

    void MaterialPassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/material.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.GetHandle(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo{};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        struct MaterialSpecData {
            uint32_t enablePCSS;
            uint32_t pcfSamplesC0;
            uint32_t pcfSamplesC1;
            uint32_t pcfSamplesC2;
        } specData = {
            Constants::ENABLE_PCSS,
            Constants::PCF_SAMPLES_CASCADE_0,
            Constants::PCF_SAMPLES_CASCADE_1,
            Constants::PCF_SAMPLES_CASCADE_2
        };

        std::array<VkSpecializationMapEntry, 4> specEntries{};
        specEntries[0] = {0, offsetof(MaterialSpecData, enablePCSS), sizeof(uint32_t)};
        specEntries[1] = {1, offsetof(MaterialSpecData, pcfSamplesC0), sizeof(uint32_t)};
        specEntries[2] = {2, offsetof(MaterialSpecData, pcfSamplesC1), sizeof(uint32_t)};
        specEntries[3] = {3, offsetof(MaterialSpecData, pcfSamplesC2), sizeof(uint32_t)};

        VkSpecializationInfo specInfo{};
        specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
        specInfo.pMapEntries = specEntries.data();
        specInfo.dataSize = sizeof(MaterialSpecData);
        specInfo.pData = &specData;

        computeStageInfo.pSpecializationInfo = &specInfo;

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        ENGINE_VERIFY(vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS,
            "MaterialPassNode: Failed to create material compute pipeline");
        vkDestroyShaderModule(device.GetHandle(), compModule, nullptr);
    }
} // namespace Engine

