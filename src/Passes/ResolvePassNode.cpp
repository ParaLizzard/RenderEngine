#include "ResolvePassNode.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    ResolvePassNode::ResolvePassNode(Device& device, Renderer& renderer)
        : device(device), renderer(renderer)
    {
        descriptorPool = LveDescriptorPool::Builder(device)
            .setMaxSets(Renderer::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Renderer::MAX_FRAMES_IN_FLIGHT)
            .build();

        descriptorSetLayout = LveDescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

        descriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            descriptorPool->allocateDescriptor(descriptorSetLayout->getDescriptorSetLayout(), descriptorSets[i]);
        }

        createPipelineLayout();
        createPipeline();
    }

    ResolvePassNode::~ResolvePassNode()
    {
        vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void ResolvePassNode::setup(RenderGraphBuilder& renderGraph)
    {
        VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();

        renderGraph.createTransientImage("FinalRender", VK_FORMAT_R16G16B16A16_SFLOAT, currentExtent, 
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        renderGraph.readBuffer("CompactMaterial", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeImage("FinalRender", VK_IMAGE_LAYOUT_GENERAL, 
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void ResolvePassNode::resolve(RenderGraph& graph, const FrameInfo& frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);

        VkDescriptorBufferInfo materialBufferInfo = graph.getBufferInfo("CompactMaterial", frameInfo.frameIndex);

        VkDescriptorImageInfo outputImageInfo{};
        outputImageInfo.imageView = graph.getImageView("FinalRender");
        outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        LveDescriptorWriter(*descriptorSetLayout, *descriptorPool)
            .writeBuffer(0, &materialBufferInfo)
            .writeImage(1, &outputImageInfo)
            .overwrite(descriptorSets[frameInfo.frameIndex]);
    }

    void ResolvePassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, 
                                &descriptorSets[frameInfo.frameIndex], 0, nullptr);

        PushConstants pc{};
        pc.frameWidth = frameInfo.extent.width;
        pc.frameHeight = frameInfo.extent.height;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

        uint32_t groupCountX = (frameInfo.extent.width + 7) / 8;
        uint32_t groupCountY = (frameInfo.extent.height + 7) / 8;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    void ResolvePassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkDescriptorSetLayout setLayouts[] = { descriptorSetLayout->getDescriptorSetLayout() };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = setLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);
    }

    void ResolvePassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/resolve.comp.spv");
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

        vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
    }
}