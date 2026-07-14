#pragma once
#include <fstream>
#include <vulkan/vulkan.h>
#include "Scene/FrameInfo.h"

namespace Engine {
    class RenderGraphBuilder;
    class RenderGraph;

    class RenderPassNode
    {
    public:
        virtual ~RenderPassNode() = default;
        virtual void setup(RenderGraphBuilder &renderGraph) = 0;
        virtual void resolve(RenderGraph &graph, const FrameInfo &frameInfo)
        {}
        virtual void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) = 0;

        virtual void markSceneDirty(){}
    };
} // namespace Engine
