#include "ResolvePassNode.h"
#include "Renderer/ShaderUtils.h"

#include <stdexcept>
#include <string>

namespace Engine
{
    namespace
    {
        void vkCheck(VkResult result, const char* what)
        {
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error(std::string("ResolvePassNode: ") + what + " failed with VkResult " + std::to_string(result));
            }
        }
    }

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

        createDescriptorResources();
        createPipelineLayout();
        createPipeline();
    }

    ResolvePassNode::~ResolvePassNode()
    {
        if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void ResolvePassNode::createDescriptorResources()
    {
        descriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            const bool allocated = descriptorPool->allocateDescriptor(descriptorSetLayout->getDescriptorSetLayout(), descriptorSets[i]);
            if (!allocated)
            {
                throw std::runtime_error("ResolvePassNode: failed to allocate descriptor set for frame " + std::to_string(i));
            }
        }
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

        const uint32_t groupCountX = (frameInfo.extent.width + 7) / 8;
        const uint32_t groupCountY = (frameInfo.extent.height + 7) / 8;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    void ResolvePassNode::createPipelineLayout()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkDescriptorSetLayout setLayouts[] = { descriptorSetLayout->getDescriptorSetLayout() };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = setLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        vkCheck(vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    }

    void ResolvePassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/resolve.comp.spv");
        VkShaderModule compModule = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPipelineShaderStageCreateInfo computeStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageInfo.module = compModule;
        computeStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeStageInfo;

        VkResult result = vkCreateComputePipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr, &pipeline);
        vkDestroyShaderModule(device.getDevice(), compModule, nullptr);
        vkCheck(result, "vkCreateComputePipelines");
    }
}