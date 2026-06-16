#pragma once

#include "Core/Device.h"
#include "Scene/Model.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Swapchain.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Passes/LightPassNode.h"
#include <stdexcept>
#include <fstream>
#include <vector>
#include <array>
#include <future>

#include "Core/JobSystem.h"
#include "Renderer/ShaderUtils.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <Core/Descriptor.h>

#include "Renderer/ResourceHeap.h"

namespace Engine
{
    class Renderer;

    struct ForwardPushConstants
    {
        glm::mat4 viewProjection{1.0f};
        uint32_t debugIsTransparent;
        uint32_t ssao;
    };

    class GBufferPassNode : public RenderPassNode
    {
    public:
        GBufferPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap, LightPassNode& prepassNode);
        ~GBufferPassNode() override;

        GBufferPassNode(const GBufferPassNode&) = delete;
        GBufferPassNode& operator=(const GBufferPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(RenderGraph& graph, const FrameInfo& frameInfo) override;
    private:
        void createPipelineLayout();
        void createPipeline();

        Device& device;
        Renderer& renderer;
        Model& megaBuffer;
        ResourceHeap& resourceHeap;
        LightPassNode& prepassNode;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
        VkPipeline transparentPipeline = VK_NULL_HANDLE;
        VkPipeline graphicsPipelineDoubleSided = VK_NULL_HANDLE;
        VkPipeline transparentPipelineDoubleSided = VK_NULL_HANDLE;

        std::unique_ptr<LveDescriptorPool> descriptorPool;
        std::unique_ptr<LveDescriptorSetLayout> ssaoSetLayout;
        std::vector<VkDescriptorSet> ssaoDescriptorSets;
        VkSampler ssaoSampler{VK_NULL_HANDLE};
        VkSampler linearSampler{VK_NULL_HANDLE};
    };
}
