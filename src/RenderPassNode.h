#pragma once
#include <vulkan/vulkan.h>
#include "FrameInfo.h"

namespace Engine
{
    class RenderGraphBuilder;

    class RenderPassNode
    {
    public:
        virtual ~RenderPassNode() = default;
        virtual void setup(RenderGraphBuilder& renderGraph) = 0;
        virtual void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) = 0;
    };
}