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

    struct ObjectData
    {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 boundingSphere;
    };

    class CullPassNode : public RenderPassNode
    {
    public:
        static constexpr uint32_t MAX_OBJECTS = 100000;

        CullPassNode(Device& device, Renderer& renderer, Model& megaBuffer);
        ~CullPassNode();

        CullPassNode(const CullPassNode&) = delete;
        CullPassNode& operator=(const CullPassNode&) = delete;
        CullPassNode(CullPassNode&&) = delete;
        CullPassNode& operator=(CullPassNode&&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(RenderGraph& graph, const FrameInfo& frameInfo) override;

        void markSceneDirty() { sceneDirty = true; }

        VkBuffer getCompactedIndirectBuffer(uint32_t frameIdx) const { return gpuCompactedIndirectCommandBuffers.at(frameIdx)->getBuffer(); }
        VkBuffer getDrawCountBuffer(uint32_t frameIdx) const { return gpuDrawCountBuffers.at(frameIdx)->getBuffer(); }
        VkBuffer getGpuObjectBuffer(uint32_t frameIdx) const { return gpuObjectSSBOs.at(frameIdx)->getBuffer(); }
        VkBuffer getSourceCommandBuffer(uint32_t frameIdx) const { return gpuSourceCommandBuffers.at(frameIdx)->getBuffer(); }
        VkDescriptorSet getObjectDescriptorSet(uint32_t frameIdx) const { return objectDescriptorSets.at(frameIdx); }
        VkDescriptorSetLayout getObjectSetLayout() const { return objectSetLayout; }
        uint32_t getActiveObjectCount() const { return static_cast<uint32_t>(objectDataArray.size()); }

    private:
        void createDescriptorResources();
        void createPerFrameBuffers();
        void createPipeline();
        void rebuildSceneArrays(const FrameInfo& info);
        void uploadFrameData(uint32_t frame, VkCommandBuffer cmd);
        void dispatchCull(uint32_t frame, VkCommandBuffer cmd, const FrameInfo& info);

        Device& device;
        Model& megaBuffer;
        Renderer& renderer;

        VkPipeline computePipeline{VK_NULL_HANDLE};
        VkPipelineLayout computePipelineLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout objectSetLayout{VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> objectDescriptorSets;

        std::vector<ObjectData> objectDataArray;
        std::vector<VkDrawIndexedIndirectCommand> indirectCommandsArray;

        std::vector<std::unique_ptr<Buffer>> cpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> gpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> cpuIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuSourceCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuCompactedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers;

        bool sceneDirty = true;
        bool dataUploadedThisFrame = false;
    };
}