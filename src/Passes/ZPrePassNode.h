#pragma once
#include "Core/Device.h"
#include "Scene/Model.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/ResourceHeap.h"

namespace Engine
{
    class Renderer;

    class ZPrePassNode : public RenderPassNode
    {
    public:
        ZPrePassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap);
        ~ZPrePassNode() override;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;

    private:
        void createPipelineLayout();
        void createPipelines();

        Device& device;
        Renderer& renderer;
        Model& megaBuffer;
        ResourceHeap& resourceHeap;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline opaquePipeline{VK_NULL_HANDLE};
        VkPipeline maskedPipeline{VK_NULL_HANDLE};
    };
}