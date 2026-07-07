#pragma once
#include "Renderer/Renderer.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/ResourceHeap.h"
#include "Core/Descriptor.h"


namespace Engine
{
    struct MaterialPushConstants
    {
        glm::mat4 viewProj;
        glm::mat4 view;
        glm::vec3 cameraPos;
        glm::uint frameWidth;
    };

    /*struct CompactMaterial {
        uint32_t packedNormal;
        uint32_t packedRadiance;
        uint32_t packedAO;
        uint32_t padding;
    };*/

    struct WorldData {
        glm::vec3 worldPos;
        float pad;
    };

    struct GPUMeshInfo {
        uint32_t firstIndex;
        int32_t  vertexOffset;
    };

    class MaterialPassNode : public RenderPassNode
    {
    public:
        MaterialPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap, RenderGraph& renderGraph);
        ~MaterialPassNode();

        MaterialPassNode(const MaterialPassNode&) = delete;
        MaterialPassNode& operator=(const MaterialPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(RenderGraph& graph, const FrameInfo& frameInfo) override;

        /*VkBuffer getCompactMaterialBuffer(size_t frameIndex) {
            return compactMaterialBuffers[frameIndex]->getBuffer();
        }*/

        [[nodiscard]] VkBuffer getPackedNormalBuffer(size_t frameIndex) const {
            return packedNormalBuffers[frameIndex]->getBuffer();
        }
        [[nodiscard]] VkBuffer getPackedRadianceBuffer(size_t frameIndex) const {
            return packedRadianceBuffers[frameIndex]->getBuffer();
        }

        [[nodiscard]] VkBuffer getWorldPositionBuffer(uint32_t frameIndex) const {
            return worldPositionBuffers[frameIndex]->getBuffer();
        }
    private:
        void createPipelineLayout();
        void createPipeline();

        Device& device;
        Model& megaBuffer;
        Renderer& renderer;
        ResourceHeap& resourceHeap;
        RenderGraph& renderGraph;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};

        std::unique_ptr<LveDescriptorPool> globalPool;
        std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;

        VkSampler sampler{VK_NULL_HANDLE};
        VkSampler nearestSampler = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<Buffer>> meshBuffers;
        //std::vector<std::unique_ptr<Buffer>> compactMaterialBuffers;
        std::vector<std::unique_ptr<Buffer>> packedNormalBuffers;
        std::vector<std::unique_ptr<Buffer>> packedRadianceBuffers;
        std::vector<std::unique_ptr<Buffer>> worldPositionBuffers;

        uint32_t lastWidth = 0;
        uint32_t lastHeight = 0;

        std::vector<GPUMeshInfo> cachedMeshInfos;
        bool meshInfoDirty = true;
        int framesToUpdate = 0;
    public:
        void markSceneDirty() { meshInfoDirty = true; }
    };
}
