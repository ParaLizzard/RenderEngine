#pragma once

#include <memory>

#include "Vulkan/Buffer.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include <glm/glm.hpp>

namespace Engine {
    class GameObject;
    struct ComputePushConstants
    {
        glm::mat4 viewProj;
        glm::vec4 frustumPlanes[6];
        glm::vec3 cameraPos;
        uint32_t cullFlags;
        uint32_t objectCount;
        uint32_t actualObjectCount;
        float projM11;
        uint32_t objectCapacity;
        uint32_t clipPlaneCount;
    };

    // ObjectData moved to TransformUpdatePassNode

    class CullPassNode: public RenderPassNode
    {
    public:
        CullPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~CullPassNode();


        CullPassNode(const CullPassNode &) = delete;
        CullPassNode &operator=(const CullPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
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
        // Object buffer is now managed globally by ResourceHeap and TransformUpdatePassNode
        [[nodiscard]] VkBuffer getGpuIndirectCommandBuffer(uint32_t frameIdx) const
        {
            return gpuIndirectCommandBuffers[frameIdx]->getBuffer();
        }
        [[nodiscard]] VkDescriptorSet getObjectDescriptorSet(uint32_t frameIdx) const
        {
            return objectDescriptorSets[frameIdx];
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
            return Config::MAX_SCENE_OBJECTS;
        }
        [[nodiscard]] uint32_t getActualObjectCount() const
        {
            return static_cast<uint32_t>(indirectCommandsArray.size());
        }

        [[nodiscard]] const glm::mat4& getActiveCullViewProj() const { return activeCullViewProj; }
        [[nodiscard]] const glm::vec3& getActiveCullCameraPos() const { return activeCullCameraPos; }

    private:
        void createPipeline();

        Device &device;
        Model &megaBuffer;
        Renderer &renderer;
        ResourceHeap &resourceHeap;

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

        //std::vector<std::unique_ptr<Buffer>> gpuCompactedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> compactedIndexBuffers;
        std::vector<std::unique_ptr<Buffer>> singleIndirectCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers; // old counter

        std::vector<std::unique_ptr<Buffer>> gpuDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuVisibleObjectBuffers;
        
        
        std::vector<std::unique_ptr<Buffer>> triangleDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> visibleMeshletBuffers;

        std::vector<std::unique_ptr<Buffer>> taskWorkgroupBuffers;
        std::vector<std::unique_ptr<Buffer>> taskDispatchCommandBuffers;

        bool sceneDirty = true;
        int framesToUpdate = 0;

        glm::mat4 activeCullViewProj{1.0f};
        glm::vec3 activeCullCameraPos{0.0f};
    };
} // namespace Engine