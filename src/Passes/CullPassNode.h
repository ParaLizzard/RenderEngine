#pragma once
#include "Renderer/Renderer.h"
#include "Renderer/RenderPassNode.h"

namespace Engine
{
    struct ComputePushConstants
    {
        glm::mat4 viewProjection;
        uint32_t objectCount;
    };

    struct ObjectData {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 boundingSphere;
    };

    class CullPassNode : public RenderPassNode
    {
    public:
        CullPassNode(Device& device, Renderer& renderer, Model& megaBuffer);
        ~CullPassNode();

        CullPassNode(const CullPassNode&) = delete;
        CullPassNode& operator=(const CullPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(const RenderGraph& graph, const FrameInfo& frameInfo) override;

        void markSceneDirty() { sceneDirty = true; }

        [[nodiscard]] VkBuffer getCompactedIndirectBuffer(uint32_t frameIdx) const { return gpuCompactedIndirectCommandBuffers[frameIdx]->getBuffer(); }
        [[nodiscard]] VkBuffer getDrawCountBuffer(uint32_t frameIdx) const { return gpuDrawCountBuffers[frameIdx]->getBuffer(); }
        [[nodiscard]] VkBuffer getGpuObjectBuffer(uint32_t frameIdx) const {return gpuObjectSSBOs[frameIdx]->getBuffer();}
        [[nodiscard]] VkDescriptorSet getObjectDescriptorSet(uint32_t frameIdx) const { return objectDescriptorSets[frameIdx]; }
        [[nodiscard]] uint32_t getMaxObjectCount() const { return static_cast<uint32_t>(objectDataArray.size()); }
    private:
        void createPipeline();

        Device& device;
        Model& megaBuffer;
        Renderer& renderer;

        VkPipeline computePipeline{VK_NULL_HANDLE};;
        VkPipelineLayout computePipelineLayout{VK_NULL_HANDLE};

        VkDescriptorSetLayout objectSetLayout{VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> objectDescriptorSets;

        std::vector<ObjectData> objectDataArray;
        std::vector<VkDrawIndexedIndirectCommand> indirectCommandsArray;
        std::vector<const GameObject*> opaqueDraws;

        std::vector<std::unique_ptr<Buffer>> cpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> gpuObjectSSBOs;

        std::vector<std::unique_ptr<Buffer>> cpuIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuIndirectCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuCompactedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers;

        bool sceneDirty = true;
        int framesToUpdate = 0;
    };
}
