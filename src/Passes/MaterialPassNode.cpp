//
// Created by Martin Varga on 15.06.2026.
//

#include "MaterialPassNode.h"

#include "Renderer/RenderGraph.h"

namespace Engine
{
    MaterialPassNode::MaterialPassNode(Device& device, Renderer& renderer, Model& megaBuffer):device(device),renderer(renderer),megaBuffer(megaBuffer)
    {


        createPipelineLayout();
        createPipeline();
    }

    MaterialPassNode::~MaterialPassNode()
    {
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void MaterialPassNode::setup(RenderGraphBuilder& renderGraph)
    {
        renderGraph.readBuffer("VisBuffer", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        renderGraph.readImage("DepthImage",VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

        renderGraph.writeBuffer("CompactMaterial", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        renderGraph.writeBuffer("WorldPosition", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void MaterialPassNode::execute(VkCommandBuffer& cmd, FrameInfo& frameInfo)
    {
    }

    void MaterialPassNode::resolve(const RenderGraph& graph, const FrameInfo& frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }

    void MaterialPassNode::createPipelineLayout()
    {
    }

    void MaterialPassNode::createPipeline()
    {
    }
}
