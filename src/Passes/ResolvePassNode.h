#pragma once

#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Core/Descriptor.h"
#include <memory>
#include <vector>

namespace Engine
{
    class ResolvePassNode : public RenderPassNode
    {
    public:
        ResolvePassNode(Device& device, Renderer& renderer);
        ~ResolvePassNode() override;

        ResolvePassNode(const ResolvePassNode&) = delete;
        ResolvePassNode& operator=(const ResolvePassNode&) = delete;
        ResolvePassNode(ResolvePassNode&&) = delete;
        ResolvePassNode& operator=(ResolvePassNode&&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(RenderGraph& graph, const FrameInfo& frameInfo) override;

    private:
        void createDescriptorResources();
        void createPipelineLayout();
        void createPipeline();

        Device& device;
        Renderer& renderer;

        std::unique_ptr<LveDescriptorPool> descriptorPool;
        std::unique_ptr<LveDescriptorSetLayout> descriptorSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};

        struct PushConstants {
            uint32_t frameWidth;
            uint32_t frameHeight;
        };
    };
}