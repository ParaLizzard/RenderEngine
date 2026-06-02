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
        uint32_t debugIsTransparent;
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

        //VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        Device& device;
        Renderer& renderer;
        Model& megaBuffer;
        ResourceHeap& resourceHeap;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
        VkPipeline transparentPipeline = VK_NULL_HANDLE;
        VkPipeline graphicsPipelineDoubleSided = VK_NULL_HANDLE;
        VkPipeline transparentPipelineDoubleSided = VK_NULL_HANDLE;
    };
}
