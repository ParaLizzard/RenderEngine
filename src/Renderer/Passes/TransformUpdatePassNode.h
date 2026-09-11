#pragma once

#include <memory>
#include <vector>

#include "Vulkan/Buffer.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Renderer.h"

namespace Engine {
    class VulkanDevice;

    struct ObjectData
    {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 boundingSphere;
        uint32_t baseMeshlet;
        uint32_t meshletCount;
        uint32_t alphaMode;
        uint32_t materialId;
    };

    class TransformUpdatePassNode: public RenderPassNode
    {
    public:
        TransformUpdatePassNode(VulkanDevice &device, Renderer &renderer);
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
            return globalObjectBuffers[renderer.getFrameIndex()];
        }

        [[nodiscard]] const std::vector<std::shared_ptr<Buffer>>& getGlobalObjectBuffers() const
        {
            return globalObjectBuffers;
        }

        [[nodiscard]] std::shared_ptr<Buffer> getGlobalObjectBuffer(uint32_t frameIndex) const
        {
            return globalObjectBuffers[frameIndex];
        }

    private:
        VulkanDevice &device;
        Renderer &renderer;

        std::vector<ObjectData> objectDataArray;
        std::vector<std::shared_ptr<Buffer>> globalObjectBuffers;
        std::vector<std::unique_ptr<Buffer>> stagingBuffers;

        bool sceneDirty = true;
        int framesToUpdate = 0;
    };
} // namespace Engine
