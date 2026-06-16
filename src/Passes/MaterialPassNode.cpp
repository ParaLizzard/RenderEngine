#include "MaterialPassNode.h"

#include <array>

#include "Core/Descriptor.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    MaterialPassNode::MaterialPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap, RenderGraph& renderGraph) :
        device(device),
        renderer(renderer),
        megaBuffer(megaBuffer),
        resourceHeap(resourceHeap),
        renderGraph(renderGraph)
    {
        globalPool = LveDescriptorPool::Builder(device)
                     .setMaxSets(Renderer::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT * 7)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Renderer::MAX_FRAMES_IN_FLIGHT * 2)
                     .build();

        globalSetLayout = LveDescriptorSetLayout::Builder(device)
                          // Vertex Position Buffer
                          .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // Index Buffer
                          .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // Object Metadata Lookup Buffer
                          .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // VisBuffer Texture Input
                          .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // Depth Texture Input
                          .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // Compact Material Output Buffer
                          .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // World Position Output Buffer
                          .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          // Vertex Attributes Buffer (UV, Normal, Tangent)
                          .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .build();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.pNext = VK_NULL_HANDLE;
        if (vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("MaterialPassNode: Failed to create texture sampler");
        }

        VkSamplerCreateInfo nearestSamplerInfo{};
        nearestSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        nearestSamplerInfo.magFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.minFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        nearestSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.maxAnisotropy = 1.0f;
        nearestSamplerInfo.pNext = VK_NULL_HANDLE;
        if (vkCreateSampler(device.getDevice(), &nearestSamplerInfo, nullptr, &nearestSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("MaterialPassNode: Failed to create nearest texture sampler");
        }

        const uint32_t MAX_SCENE_OBJECTS = 100000;
        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();
        lastWidth = extent.width;
        lastHeight = extent.height;
        uint32_t initialPixelCount = lastWidth * lastHeight;

        meshBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        worldPositionBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        compactMaterialBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            meshBuffers[i] = std::make_unique<Buffer>(
                device, sizeof(GPUMeshInfo), MAX_SCENE_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0
            );

            worldPositionBuffers[i] = std::make_unique<Buffer>(
                device, sizeof(WorldData), initialPixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0
            );

            compactMaterialBuffers[i] = std::make_unique<Buffer>(
                device, sizeof(CompactMaterial), initialPixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0
            );
        }

        descriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (!globalPool->allocateDescriptor(globalSetLayout->getDescriptorSetLayout(), descriptorSets[i]))
            {
                throw std::runtime_error("MaterialPassNode: Failed to allocate descriptor sets!");
            }
        }

        createPipelineLayout();
        createPipeline();
    }

    MaterialPassNode::~MaterialPassNode()
    {
        if (nearestSampler != VK_NULL_HANDLE)
            vkDestroySampler(device.getDevice(), nearestSampler, nullptr);
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(device.getDevice(), sampler, nullptr);
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void MaterialPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        renderGraph.writeBuffer("CompactMaterial",
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             VK_ACCESS_2_SHADER_WRITE_BIT);

        renderGraph.readBuffer("CullObjectData", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("VisBuffer", VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeBuffer("CompactMaterial", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeBuffer("WorldPosition", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void MaterialPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();
        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();

        if (extent.width != lastWidth || extent.height != lastHeight)
        {
            lastWidth = extent.width;
            lastHeight = extent.height;
            uint32_t pixelCount = lastWidth * lastHeight;

            for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
            {
                worldPositionBuffers[i] = std::make_unique<Buffer>(
                    device, sizeof(WorldData), pixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    , 0);

                compactMaterialBuffers[i] = std::make_unique<Buffer>(
                    device, sizeof(CompactMaterial), pixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    , 0);

                VkDescriptorImageInfo depthImageInfo{};
                depthImageInfo.imageView = renderGraph.getImageView("DepthImage");
                depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                depthImageInfo.sampler = sampler;

                VkDescriptorImageInfo visBufferInfo{};
                visBufferInfo.imageView = renderGraph.getImageView("VisBuffer");
                visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                visBufferInfo.sampler = nearestSampler;

                VkDescriptorBufferInfo vertexBufferInfo = megaBuffer.getPositionBuffer()->descriptorInfo(
                    VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo indexBufferInfo = megaBuffer.getIndexBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo meshBufferInfo = meshBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo compactBufferInfo = compactMaterialBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo positionBufferInfo = worldPositionBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo attributeBufferInfo = megaBuffer.getAttributeBuffer()->descriptorInfo(
                    VK_WHOLE_SIZE, 0);

                LveDescriptorWriter(*globalSetLayout, *globalPool)
                    .writeBuffer(0, &vertexBufferInfo)
                    .writeBuffer(1, &indexBufferInfo)
                    .writeBuffer(2, &meshBufferInfo)
                    .writeImage(3, &visBufferInfo)
                    .writeImage(4, &depthImageInfo)
                    .writeBuffer(5, &compactBufferInfo)
                    .writeBuffer(6, &positionBufferInfo)
                    .writeBuffer(7, &attributeBufferInfo)
                    .overwrite(descriptorSets[i]);
            }
        }

        std::vector<GPUMeshInfo> meshInfos;
        meshInfos.reserve(frameInfo.gameObjects->size());

        for (const auto& obj : *frameInfo.gameObjects)
        {
            if (obj.subMesh.indexCount == 0) continue;
            if (obj.alphaMode == AlphaMode::Blend) continue;

            GPUMeshInfo info{};
            info.firstIndex = obj.subMesh.firstIndex;
            info.vertexOffset = obj.subMesh.vertexOffset;
            meshInfos.push_back(info);
        }

        if (!meshInfos.empty())
        {
            meshBuffers[currentFrame]->writeToBuffer(meshInfos.data(), meshInfos.size() * sizeof(GPUMeshInfo), 0);
            meshBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet();
        VkDescriptorSet sets[] = {bindlessSet, descriptorSets[currentFrame]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);


        glm::mat4 projection = frameInfo.camera->getProjection();
        glm::mat4 view = frameInfo.camera->getView();

        glm::mat4 clipMatrix = glm::mat4(1.0f);
        glm::mat4 viewProjection = clipMatrix * projection * view;

        MaterialPushConstants pc{};
        pc.viewProj = viewProjection;
        pc.view = view;
        pc.cameraPos = glm::vec3(glm::inverse(view)[3]);
        pc.frameWidth = extent.width;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MaterialPushConstants), &pc);

        uint32_t groupCountX = (extent.width + 15) / 16;
        uint32_t groupCountY = (extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    void MaterialPassNode::resolve(RenderGraph& graph, const FrameInfo& frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        uint32_t currentFrame = frameInfo.frameIndex;

        VkDescriptorImageInfo depthImageInfo{};
        depthImageInfo.imageView = graph.getImageView("DepthImage");
        depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthImageInfo.sampler = sampler;

        VkDescriptorImageInfo visBufferInfo{};
        visBufferInfo.imageView = graph.getImageView("VisBuffer");
        visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        visBufferInfo.sampler = nearestSampler;

        VkDescriptorBufferInfo vertexBufferInfo = megaBuffer.getPositionBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo indexBufferInfo = megaBuffer.getIndexBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo meshBufferInfo = meshBuffers[currentFrame]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo compactBufferInfo = compactMaterialBuffers[currentFrame]->descriptorInfo(
            VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo positionBufferInfo = worldPositionBuffers[currentFrame]->
            descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo attributeBufferInfo = megaBuffer.getAttributeBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo objectBufferInfo = renderGraph.getBufferInfo("CullObjectData", currentFrame);

        LveDescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &vertexBufferInfo)
            .writeBuffer(1, &indexBufferInfo)
            .writeBuffer(2, &meshBufferInfo)
            .writeImage(3, &visBufferInfo)
            .writeImage(4, &depthImageInfo)
            .writeBuffer(5, &compactBufferInfo)
            .writeBuffer(6, &positionBufferInfo)
            .writeBuffer(7, &attributeBufferInfo)
            .writeBuffer(8, &objectBufferInfo)
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
        pipelineLayoutInfo.setLayoutCount = 2; // set 0: Bindless textures/materials, set 1: Pass-specific storage
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("MaterialPassNode: Failed to create compute pipeline layout");
        }
    }

    void MaterialPassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/material.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo{};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("MaterialPassNode: Failed to create material compute pipeline");
        }
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }
}
