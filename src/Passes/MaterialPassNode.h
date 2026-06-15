#pragma once
#include "Renderer/Renderer.h"
#include "Renderer/RenderPassNode.h"


namespace Engine
{
    class MaterialPassNode : public RenderPassNode
    {
    public:
        MaterialPassNode(Device& device, Renderer& renderer, Model& megaBuffer);
        ~MaterialPassNode();

        MaterialPassNode(const MaterialPassNode&) = delete;
        MaterialPassNode& operator=(const MaterialPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(const RenderGraph& graph, const FrameInfo& frameInfo) override;
    private:
        void createPipelineLayout();
        void createPipeline();

        Device& device;
        Model& megaBuffer;
        Renderer& renderer;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };
}
