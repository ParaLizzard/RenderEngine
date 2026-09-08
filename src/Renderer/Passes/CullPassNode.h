#pragma once

#include <memory>
#include <vector>

#include "Vulkan/Buffer.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include "Core/EngineConstants.h"
#include <glm/glm.hpp>

namespace Engine {
    class GameObject;
    struct ComputePushConstants
    {
        glm::mat4 view;
        glm::vec4 projParams;
        glm::vec4 hizParams;
        glm::vec2 screenParams;
        uint32_t cullFlags;
        uint32_t objectCount;
        uint32_t actualObjectCount;
        uint32_t objectCapacity;
        uint32_t clipPlaneCount;
        uint32_t targetAlphaMode;
        uint32_t phase;
        uint32_t pad;
    };

    class CullPassNode: public RenderPassNode
    {
    public:
        static constexpr uint32_t WORKGROUP_SIZE = Constants::CULL_WORKGROUP_SIZE;

        CullPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, uint32_t phase = 0, CullPassNode *parentCullPass = nullptr);
        ~CullPassNode();

        CullPassNode(const CullPassNode &) = delete;
        CullPassNode &operator=(const CullPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void registerResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void updateResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        void markSceneDirty() override
        {
            sceneDirty = true;
        }

        [[nodiscard]] VkBuffer getDrawCountBuffer(uint32_t frameIdx) const
        {
            return gpuDrawCountBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getGpuIndirectCommandBuffer(uint32_t frameIdx) const
        {
            return gpuIndirectCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkDescriptorSet getObjectDescriptorSet(uint32_t frameIdx) const
        {
            return objectDescriptorSets[frameIdx];
        }
        [[nodiscard]] VkDescriptorSet getMaskedObjectDescriptorSet(uint32_t frameIdx) const
        {
            return maskedObjectDescriptorSets[frameIdx];
        }
        VkDescriptorSetLayout getObjectSetLayout()
        {
            return objectSetLayout;
        }
        [[nodiscard]] VkBuffer getCompactedIndexBuffer(uint32_t frameIdx) const
        {
            return compactedIndexBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getSingleIndirectCommandBuffer(uint32_t currentFrame) const
        {
            return singleIndirectCommandBuffers[currentFrame]->getBuffer();
        }
        [[nodiscard]] VkBuffer getMaskedCompactedIndexBuffer(uint32_t frameIdx) const
        {
            return maskedCompactedIndexBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getMaskedSingleIndirectCommandBuffer(uint32_t currentFrame) const
        {
            return maskedSingleIndirectCommandBuffers[currentFrame]->getBuffer();
        }

        [[nodiscard]] void* getSingleIndirectCommandMapped(uint32_t frameIdx) const
        {
            return singleIndirectCommandBuffers[frameIdx]->getMappedMemory();
        }
        [[nodiscard]] void* getGpuDispatchCommandMapped(uint32_t frameIdx) const
        {
            return gpuDispatchCommandBuffers[frameIdx]->getMappedMemory();
        }
        [[nodiscard]] VkBuffer getTaskDispatchCommandBuffer(uint32_t frameIdx) const
        {
            return taskDispatchCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] uint32_t getMaxObjectCount() const
        {
            return Constants::MAX_SCENE_OBJECTS;
        }
        [[nodiscard]] uint32_t getActualObjectCount() const
        {
            return static_cast<uint32_t>(indirectCommandsArray.size());
        }
        [[nodiscard]] VkBuffer getMaskedIndirectCommandBuffer(uint32_t frameIdx) const
        {
            return gpuMaskedIndirectCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] uint32_t getMaskedActualDrawCount() const
        {
            return static_cast<uint32_t>(maskedIndirectCommandsArray.size());
        }

        [[nodiscard]] VkBuffer getCandidateDispatchCommandBuffer(uint32_t frameIdx) const
        {
            return gpuCandidateDispatchCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getCandidateObjectBuffer(uint32_t frameIdx) const
        {
            return gpuCandidateObjectBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getMaskedCandidateDispatchCommandBuffer(uint32_t frameIdx) const
        {
            return gpuMaskedCandidateDispatchCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkBuffer getMaskedCandidateObjectBuffer(uint32_t frameIdx) const
        {
            return gpuMaskedCandidateObjectBuffers[frameIdx]->getBuffer();
        }

        [[nodiscard]] uint32_t getPhase() const { return phase; }
        [[nodiscard]] const glm::mat4& getActiveCullViewProj() const { return activeCullViewProj; }
        [[nodiscard]] const glm::vec3& getActiveCullCameraPos() const { return activeCullCameraPos; }

    private:
        void createPipeline();

        Device &device;
        Model &megaBuffer;
        Renderer &renderer;
        ResourceHeap &resourceHeap;
        uint32_t phase = 0;
        CullPassNode *parentCullPass = nullptr;

        VkPipeline objectCullPipeline {VK_NULL_HANDLE};
        VkPipeline taskSubmitPipeline {VK_NULL_HANDLE};

        VkPipeline meshletCullPipeline {VK_NULL_HANDLE};
        VkPipeline triangleCullPipeline {VK_NULL_HANDLE};
        VkPipelineLayout computePipelineLayout {VK_NULL_HANDLE};

        VkDescriptorSetLayout objectSetLayout {VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool {VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> objectDescriptorSets;

        std::vector<VkDrawIndexedIndirectCommand> indirectCommandsArray;
        std::vector<const GameObject *> opaqueDraws;

        std::vector<std::unique_ptr<Buffer>> gpuIndirectCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> compactedIndexBuffers;
        std::vector<std::unique_ptr<Buffer>> singleIndirectCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuVisibleObjectBuffers;
        
        std::vector<std::unique_ptr<Buffer>> triangleDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> visibleMeshletBuffers;

        std::vector<std::unique_ptr<Buffer>> taskWorkgroupBuffers;
        std::vector<std::unique_ptr<Buffer>> taskDispatchCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuCandidateDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuCandidateObjectBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuMaskedCandidateDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuMaskedCandidateObjectBuffers;

        std::vector<const GameObject *> maskedDraws;
        std::vector<VkDrawIndexedIndirectCommand> maskedIndirectCommandsArray;
        std::vector<std::unique_ptr<Buffer>> gpuMaskedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuMaskedDrawCountBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuMaskedDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuMaskedVisibleObjectBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedCompactedIndexBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedSingleIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedTriangleDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedVisibleMeshletBuffers;
        std::vector<VkDescriptorSet> maskedObjectDescriptorSets;

        bool sceneDirty = true;
        int framesToUpdate = 0;

        VkSampler hizSampler {VK_NULL_HANDLE};
        glm::mat4 activeCullView{1.0f};
        glm::mat4 activeCullViewProj{1.0f};
        glm::vec3 activeCullCameraPos{0.0f};
    };
}