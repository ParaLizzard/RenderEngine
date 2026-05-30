#pragma once

#include "Core/Device.h"
#include "Scene/Model.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Swapchain.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>

#include "Renderer/ResourceHeap.h"

namespace Engine
{
    class Renderer;

    struct ForwardPushConstants
    {
        glm::mat4 modelMatrix{1.0f};
        glm::mat4 viewProjection{1.0f};
    };

    class ForwardPassNode : public RenderPassNode
    {
    public:
        ForwardPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap);
        ~ForwardPassNode() override;

        ForwardPassNode(const ForwardPassNode&) = delete;
        ForwardPassNode& operator=(const ForwardPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;

    private:
        void createPipelineLayout();
        void createPipeline();



        Device& engineDevice;
        Renderer& engineRenderer;
        Model& geometryMegaBuffer;
        ResourceHeap& engineResourceHeap;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
    };
}
