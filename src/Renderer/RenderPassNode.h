#pragma once
#include <fstream>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
#include "Renderer/FrameInfo.h"

namespace Engine {
    class RenderGraphBuilder;
    class RenderGraph;

    class RenderPassNode
    {
    public:
        explicit RenderPassNode(std::string name = "Unnamed Pass")
            : passName(std::move(name))
        {}
        virtual ~RenderPassNode() = default;

        virtual void setup(RenderGraphBuilder &renderGraph) = 0;
        virtual void resolve(RenderGraph &graph, const FrameInfo &frameInfo)
        {}
        virtual void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) = 0;

        virtual void markSceneDirty(){}

        [[nodiscard]] const std::string &getName() const { return passName; }

    private:
        std::string passName;
    };
} // namespace Engine
