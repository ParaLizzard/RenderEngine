#pragma once
#include "CullPassNode.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderPassNode.h"

namespace Engine
{
    struct VisbilityPushConstants
    {
        glm::mat4 viewProjection;
    };

    class VisibilityPassNode : public RenderPassNode
    {
    public:
        VisibilityPassNode(Device& device, Renderer& renderer, Model& megaBuffer, CullPassNode& cullPass);
        ~VisibilityPassNode();

        VisibilityPassNode(const VisibilityPassNode&) = delete;
        VisibilityPassNode& operator=(const VisibilityPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(const RenderGraph& graph, const FrameInfo& frameInfo) override;


    private:
        void createPipelineLayout();
        void createPipeline();

        Device& device;
        Model& megaBuffer;
        Renderer& renderer;
        CullPassNode& cullPass;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    };
}
