#pragma once

#include <memory>
#include <vector>

#include "Vulkan/Buffer.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Renderer.h"

namespace Engine {

    struct ObjectData
    {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 boundingSphere;
        uint32_t baseMeshlet;
        uint32_t meshletCount;
    };

    class TransformUpdatePassNode: public RenderPassNode
    {
    public:
        TransformUpdatePassNode(Device &device, Renderer &renderer);
        ~TransformUpdatePassNode() = default;

        TransformUpdatePassNode(const TransformUpdatePassNode &) = delete;
        TransformUpdatePassNode &operator=(const TransformUpdatePassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        void markSceneDirty() override
        {
            sceneDirty = true;
        }

        std::shared_ptr<Buffer> getGlobalObjectBuffer() const
        {
            return globalObjectBuffer;
        }

    private:
        Device &device;
        Renderer &renderer;

        std::vector<ObjectData> objectDataArray;
        std::shared_ptr<Buffer> globalObjectBuffer;
        std::vector<std::unique_ptr<Buffer>> stagingBuffers;

        bool sceneDirty = true;
        int framesToUpdate = 0;
    };
} // namespace Engine
