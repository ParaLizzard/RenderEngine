#include "MaterialPassNode.h"

#include <array>

#include "Core/Descriptor.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Renderer/ShaderUtils.h"

namespace Engine {
    MaterialPassNode::MaterialPassNode(
        Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, RenderGraph &renderGraph):
        device(device), renderer(renderer), megaBuffer(megaBuffer), resourceHeap(resourceHeap), renderGraph(renderGraph)
    {
        globalPool = DescriptorPool::Builder(device)
                         .setMaxSets(Config::MAX_FRAMES_IN_FLIGHT)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Config::MAX_FRAMES_IN_FLIGHT * 8)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Config::MAX_FRAMES_IN_FLIGHT * 3)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Config::MAX_FRAMES_IN_FLIGHT)
                         .build();

        globalSetLayout = DescriptorSetLayout::Builder(device)
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
                              // FinalRender Output Image
                              .addBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                              // SsaoBlurImage Input Texture (Previous frame)
                              .addBinding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                              .addBinding(11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                              .build();

        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.pNext = VK_NULL_HANDLE;
        if (vkCreateSampler(device.getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("MaterialPassNode: Failed to create texture sampler");
        }

        VkSamplerCreateInfo nearestSamplerInfo {};
        nearestSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        nearestSamplerInfo.magFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.minFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        nearestSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nearestSamplerInfo.maxAnisotropy = 1.0f;
        nearestSamplerInfo.pNext = VK_NULL_HANDLE;
        if (vkCreateSampler(device.getDevice(), &nearestSamplerInfo, nullptr, &nearestSampler) != VK_SUCCESS) {
            throw std::runtime_error("MaterialPassNode: Failed to create nearest texture sampler");
        }

        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();
        lastWidth = extent.width;
        lastHeight = extent.height;
        uint32_t initialPixelCount = lastWidth * lastHeight;

        meshBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        worldPositionBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        packedNormalBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        packedRadianceBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            meshBuffers[i] =
                std::make_unique<Buffer>(device,
                                         sizeof(GPUMeshInfo),
                                         Config::MAX_SCENE_OBJECTS,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         0);

            worldPositionBuffers[i] = std::make_unique<Buffer>(device,
                                                               sizeof(WorldData),
                                                               initialPixelCount,
                                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                               VMA_MEMORY_USAGE_GPU_ONLY,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                               0);

            packedNormalBuffers[i] = std::make_unique<Buffer>(device,
                                                              sizeof(uint32_t),
                                                              initialPixelCount,
                                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                              0);

            packedRadianceBuffers[i] = std::make_unique<Buffer>(device,
                                                                sizeof(uint32_t),
                                                                initialPixelCount,
                                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                VMA_MEMORY_USAGE_GPU_ONLY,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                0);
        }

        descriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            if (!globalPool->allocateDescriptor(globalSetLayout->getDescriptorSetLayout(), descriptorSets[i])) {
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

    void MaterialPassNode::setup(RenderGraphBuilder &renderGraph)
    {
        renderGraph.readBuffer("CullObjectData", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("VisBuffer",
                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
        VkExtent2D halfExtent = {currentExtent.width / 2, currentExtent.height / 2};
        renderGraph.createTransientImage("SsaoBlurImage", VK_FORMAT_R8_UNORM, halfExtent);

        renderGraph.readImage("DepthImage",
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("SsaoBlurImage",
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeBuffer("PackedNormals", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeBuffer(
            "PackedRadiances", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeBuffer("WorldPosition", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

        renderGraph.createTransientImage("FinalRender",
                                         VK_FORMAT_R16G16B16A16_SFLOAT,
                                         currentExtent,
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        renderGraph.writeImage("FinalRender",
                               VK_IMAGE_LAYOUT_GENERAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void MaterialPassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();
        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();

        if (extent.width != lastWidth || extent.height != lastHeight) {
            lastWidth = extent.width;
            lastHeight = extent.height;
            uint32_t pixelCount = lastWidth * lastHeight;

            for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
                worldPositionBuffers[i] = std::make_unique<Buffer>(device,
                                                                   sizeof(WorldData),
                                                                   pixelCount,
                                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                   0);

                VkDescriptorImageInfo depthImageInfo {};
                depthImageInfo.imageView = renderGraph.getImageView("DepthImage");
                depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                depthImageInfo.sampler = sampler;

                VkDescriptorImageInfo visBufferInfo {};
                visBufferInfo.imageView = renderGraph.getImageView("VisBuffer");
                visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                visBufferInfo.sampler = nearestSampler;

                VkDescriptorBufferInfo vertexBufferInfo =
                    megaBuffer.getPositionBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo indexBufferInfo = megaBuffer.getIndexBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo meshBufferInfo = meshBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo positionBufferInfo = worldPositionBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
                VkDescriptorBufferInfo attributeBufferInfo =
                    megaBuffer.getAttributeBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
            }
        }

        if (meshInfoDirty) {
            cachedMeshInfos.clear();
            cachedMeshInfos.reserve(frameInfo.gameObjects->size());

            for (const auto &obj: *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0)
                    continue;
                if (obj.alphaMode == AlphaMode::Blend)
                    continue;

                GPUMeshInfo info {};
                info.firstIndex = obj.subMesh.firstIndex;
                info.vertexOffset = obj.subMesh.vertexOffset;
                cachedMeshInfos.push_back(info);
            }
            meshInfoDirty = false;
            framesToUpdate = Config::MAX_FRAMES_IN_FLIGHT;
        }

        if (framesToUpdate > 0 && !cachedMeshInfos.empty()) {
            meshBuffers[currentFrame]->writeToBuffer(
                cachedMeshInfos.data(), cachedMeshInfos.size() * sizeof(GPUMeshInfo), 0);
            meshBuffers[currentFrame]->flush(VK_WHOLE_SIZE, 0);
            framesToUpdate--;
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        VkDescriptorSet bindlessSet = resourceHeap.getDescriptorSet(currentFrame);
        VkDescriptorSet sets[] = {bindlessSet, descriptorSets[currentFrame]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);


        glm::mat4 projection = frameInfo.camera->getProjection();
        glm::mat4 view = frameInfo.camera->getView();

        glm::mat4 clipMatrix = glm::mat4(1.0f);
        glm::mat4 viewProjection = clipMatrix * projection * view;

        MaterialPushConstants pc {};
        pc.viewProj = viewProjection;
        pc.view = view;
        pc.cameraPos = glm::vec3(glm::inverse(view)[3]);
        pc.frameWidth = extent.width;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MaterialPushConstants), &pc);

        uint32_t groupCountX = (extent.width + 15) / 16;
        uint32_t groupCountY = (extent.height + 15) / 16;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    void MaterialPassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        VkExtent2D extent = renderer.getSwapChain().getSwapChainExtent();
        if (extent.width != lastWidth || extent.height != lastHeight) {
            lastWidth = extent.width;
            lastHeight = extent.height;
            uint32_t pixelCount = lastWidth * lastHeight;

            for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
                worldPositionBuffers[i] = std::make_unique<Buffer>(device,
                                                                   sizeof(WorldData),
                                                                   pixelCount,
                                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                   VMA_MEMORY_USAGE_GPU_ONLY,
                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                   0);
                packedNormalBuffers[i] = std::make_unique<Buffer>(device,
                                                                  sizeof(uint32_t),
                                                                  pixelCount,
                                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                  VMA_MEMORY_USAGE_GPU_ONLY,
                                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                  0);
                packedRadianceBuffers[i] = std::make_unique<Buffer>(device,
                                                                    sizeof(uint32_t),
                                                                    pixelCount,
                                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                    VMA_MEMORY_USAGE_GPU_ONLY,
                                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                    0);
            }
        }

        RenderPassNode::resolve(graph, frameInfo);

        uint32_t currentFrame = frameInfo.frameIndex;

        VkDescriptorImageInfo depthImageInfo {};
        depthImageInfo.imageView = graph.getImageView("DepthImage");
        depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthImageInfo.sampler = sampler;

        VkDescriptorImageInfo visBufferInfo {};
        visBufferInfo.imageView = graph.getImageView("VisBuffer");
        visBufferInfo.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        visBufferInfo.sampler = nearestSampler;

        VkDescriptorBufferInfo vertexBufferInfo = megaBuffer.getPositionBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo indexBufferInfo = megaBuffer.getIndexBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo meshBufferInfo = meshBuffers[currentFrame]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo normalBufferInfo = packedNormalBuffers[currentFrame]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo radianceBufferInfo =
            packedRadianceBuffers[currentFrame]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo positionBufferInfo =
            worldPositionBuffers[currentFrame]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo attributeBufferInfo = megaBuffer.getAttributeBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo objectBufferInfo = renderGraph.getBufferInfo("CullObjectData", currentFrame);

        VkDescriptorImageInfo finalRenderInfo {};
        finalRenderInfo.imageView = graph.getImageView("FinalRender");
        finalRenderInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo ssaoInfo {};
        ssaoInfo.imageView = graph.getImageView("SsaoBlurImage");
        ssaoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ssaoInfo.sampler = sampler;

        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &vertexBufferInfo)
            .writeBuffer(1, &indexBufferInfo)
            .writeBuffer(2, &meshBufferInfo)
            .writeImage(3, &visBufferInfo)
            .writeImage(4, &depthImageInfo)
            .writeBuffer(5, &normalBufferInfo)
            .writeBuffer(6, &positionBufferInfo)
            .writeBuffer(7, &attributeBufferInfo)
            .writeBuffer(8, &objectBufferInfo)
            .writeImage(9, &finalRenderInfo)
            .writeImage(10, &ssaoInfo)
            .writeBuffer(11, &radianceBufferInfo)
            .overwrite(descriptorSets[currentFrame]);
    }

    void MaterialPassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MaterialPushConstants);

        VkDescriptorSetLayout bindlessLayout = resourceHeap.getDescriptorSetLayout();
        VkDescriptorSetLayout layouts[] = {bindlessLayout, globalSetLayout->getDescriptorSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 2; // set 0: Bindless textures/materials, set 1: Pass-specific storage
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("MaterialPassNode: Failed to create compute pipeline layout");
        }
    }

    void MaterialPassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/material.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo {};
        computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        if (vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) !=
            VK_SUCCESS) {
            throw std::runtime_error("MaterialPassNode: Failed to create material compute pipeline");
        }
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }
} // namespace Engine
