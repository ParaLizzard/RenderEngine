#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/fwd.hpp>
#include <glm/gtx/transform.hpp>

#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/FrameInfo.h"

namespace Engine {
    class Renderer;

    class FxaaPassNode: public RenderPassNode
    {
    public:
        FxaaPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~FxaaPassNode() override;

        FxaaPassNode(const FxaaPassNode &) = delete;
        FxaaPassNode &operator=(const FxaaPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

    private:
        void createPipelineLayout();
        void createPipeline();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        ResourceHeap &resourceHeap;

        VkPipelineLayout pipelineLayout {VK_NULL_HANDLE};
        VkPipeline graphicsPipeline {VK_NULL_HANDLE};
        VkDescriptorSetLayout descriptorSetLayout {VK_NULL_HANDLE};
        VkDescriptorPool descriptorPool {VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> descriptorSets;
        VkSampler sampler {VK_NULL_HANDLE};

        std::vector<VkBuffer> cachedBuffers;
    };
} // namespace Engine
