#include "MaterialPassNode.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    MaterialPassNode::MaterialPassNode(Device& device, Renderer& renderer, Model& megaBuffer,
                                       ResourceHeap& resourceHeap, RenderGraph& renderGraph) :
        device(device), renderer(renderer), megaBuffer(megaBuffer),
        resourceHeap(resourceHeap), renderGraph(renderGraph)
    {
        globalPool = LveDescriptorPool::Builder(device)
                     .setMaxSets(Renderer::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT * 11)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Renderer::MAX_FRAMES_IN_FLIGHT * 2)
                     .build();

        globalSetLayout = LveDescriptorSetLayout::Builder(device)
                          .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .addBinding(10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                          .build();

        VkSamplerCreateInfo sInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sInfo.magFilter = VK_FILTER_LINEAR; sInfo.minFilter = VK_FILTER_LINEAR;
        sInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &sInfo, nullptr, &sampler);

        VkSamplerCreateInfo nInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        nInfo.magFilter = VK_FILTER_NEAREST; nInfo.minFilter = VK_FILTER_NEAREST;
        nInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        nInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device.getDevice(), &nInfo, nullptr, &nearestSampler);

        uint32_t width = renderer.getSwapChain().width();
        uint32_t height = renderer.getSwapChain().height();
        uint32_t pixelCount = width * height;

        meshBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        worldPositionBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        compactMaterialBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        binningMetaBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        pixelCoordBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            meshBuffers[i] = std::make_unique<Buffer>(device, sizeof(GPUMeshInfo), 100000, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
            worldPositionBuffers[i] = std::make_unique<Buffer>(device, sizeof(WorldData), pixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0);
            compactMaterialBuffers[i] = std::make_unique<Buffer>(device, sizeof(CompactMaterial), pixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0);
            binningMetaBuffers[i] = std::make_unique<Buffer>(device, sizeof(uint32_t) * 516, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0);
            pixelCoordBuffers[i] = std::make_unique<Buffer>(device, sizeof(uint32_t), pixelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0);
        }

        descriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
            globalPool->allocateDescriptor(globalSetLayout->getDescriptorSetLayout(), descriptorSets[i]);

        createPipelineLayout();
        createPipeline();
    }

    MaterialPassNode::~MaterialPassNode()
    {
        vkDestroySampler(device.getDevice(), sampler, nullptr);
        vkDestroySampler(device.getDevice(), nearestSampler, nullptr);
        vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        vkDestroyPipeline(device.getDevice(), classifyPipeline, nullptr);
        vkDestroyPipeline(device.getDevice(), prefixSumPipeline, nullptr);
        vkDestroyPipeline(device.getDevice(), binningPipeline, nullptr);
        vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void MaterialPassNode::setup(RenderGraphBuilder& graph)
    {
        graph.writeBuffer("CompactMaterial", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        graph.readBuffer("CullObjectData", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        graph.readImage("VisBuffer", VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        graph.readImage("DepthImage", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        graph.writeBuffer("WorldPosition", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void MaterialPassNode::resolve(RenderGraph& graph, const FrameInfo& frameInfo)
    {
        uint32_t i = frameInfo.frameIndex;

        // Create persistent local variables so the pointers are valid
        VkDescriptorImageInfo depthInfo = {sampler, graph.getImageView("DepthImage"), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo visInfo = {nearestSampler, graph.getImageView("VisBuffer"), VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};

        VkDescriptorBufferInfo vBuf = megaBuffer.getPositionBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo iBuf = megaBuffer.getIndexBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo mBuf = meshBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo cBuf = compactMaterialBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo wBuf = worldPositionBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo aBuf = megaBuffer.getAttributeBuffer()->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo oBuf = graph.getBufferInfo("CullObjectData", i);
        VkDescriptorBufferInfo bMeta = binningMetaBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
        VkDescriptorBufferInfo pCoord = pixelCoordBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

        LveDescriptorWriter writer(*globalSetLayout, *globalPool);
        writer.writeBuffer(0, &vBuf);
        writer.writeBuffer(1, &iBuf);
        writer.writeBuffer(2, &mBuf);
        writer.writeImage(3, &visInfo);
        writer.writeImage(4, &depthInfo);
        writer.writeBuffer(5, &cBuf);
        writer.writeBuffer(6, &wBuf);
        writer.writeBuffer(7, &aBuf);
        writer.writeBuffer(8, &oBuf);
        writer.writeBuffer(9, &bMeta);
        writer.writeBuffer(10, &pCoord);

        writer.overwrite(descriptorSets[i]);
    }

    void MaterialPassNode::execute(VkCommandBuffer& cmd, FrameInfo& info)
    {
        uint32_t f = info.frameIndex;
        VkExtent2D ext = info.extent;

        std::vector<GPUMeshInfo> mInfo(info.gameObjects->size());
        for(size_t i=0; i<info.gameObjects->size(); ++i) {
            mInfo[i] = { (*info.gameObjects)[i].subMesh.firstIndex, (*info.gameObjects)[i].subMesh.vertexOffset };
        }
        meshBuffers[f]->writeToBuffer(mInfo.data(), mInfo.size() * sizeof(GPUMeshInfo), 0);

        vkCmdFillBuffer(cmd, binningMetaBuffers[f]->getBuffer(), 0, VK_WHOLE_SIZE, 0);

        VkDescriptorSet sets[] = { resourceHeap.getDescriptorSet(), descriptorSets[f] };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, sets, 0, nullptr);

        MaterialPushConstants pc{info.camera->getProjection() * info.camera->getView(), glm::vec4(info.cameraPos, 1.0f), ext.width};
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, classifyPipeline);
        vkCmdDispatch(cmd, (ext.width + 7) / 8, (ext.height + 7) / 8, 1);

        VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, nullptr, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 1, &b};
        vkCmdPipelineBarrier2(cmd, &d);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefixSumPipeline);
        vkCmdDispatch(cmd, 1, 1, 1);

        vkCmdPipelineBarrier2(cmd, &d);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, binningPipeline);
        vkCmdDispatch(cmd, (ext.width + 7) / 8, (ext.height + 7) / 8, 1);

        vkCmdPipelineBarrier2(cmd, &d);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdDispatch(cmd, (ext.width * ext.height + 63) / 64, 1, 1);
    }

    void MaterialPassNode::createPipelineLayout()
    {
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MaterialPushConstants)};
        VkDescriptorSetLayout layouts[] = { resourceHeap.getDescriptorSetLayout(), globalSetLayout->getDescriptorSetLayout() };
        VkPipelineLayoutCreateInfo lInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 2, layouts, 1, &range};
        vkCreatePipelineLayout(device.getDevice(), &lInfo, nullptr, &pipelineLayout);
    }

    void MaterialPassNode::createPipeline()
    {
        auto create = [&](const char* path, VkPipeline& pipe) {
            auto code = ShaderUtils::readFile(path);
            VkShaderModule mod = ShaderUtils::createShaderModule(device.getDevice(), code);
            VkComputePipelineCreateInfo pInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, mod, "main", nullptr}, pipelineLayout, VK_NULL_HANDLE, 0};
            vkCreateComputePipelines(device.getDevice(), device.getPipelineCache(), 1, &pInfo, nullptr, &pipe);
            vkDestroyShaderModule(device.getDevice(), mod, nullptr);
        };
        create("shaders/material.comp.spv", pipeline);
        create("shaders/classify.comp.spv", classifyPipeline);
        create("shaders/binning.comp.spv", binningPipeline);
        create("shaders/prefix_sum.comp.spv", prefixSumPipeline);
    }
}