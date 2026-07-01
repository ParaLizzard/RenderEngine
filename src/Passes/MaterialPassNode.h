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
        glm::vec3 cameraPos;
        glm::uint frameWidth;
    };

    struct CompactMaterial {
        uint32_t packedNormal;
        uint32_t packedRadiance;
        uint32_t packedAO;
        uint32_t padding;
    };

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

        VkBuffer getCompactMaterialBuffer(size_t frameIndex) {
            return compactMaterialBuffers[frameIndex]->getBuffer();
        }

        VkBuffer getWorldPositionBuffer(uint32_t frameIndex) const {
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
        VkPipeline classifyPipeline{VK_NULL_HANDLE};
        VkPipeline prefixSumPipeline{VK_NULL_HANDLE};
        VkPipeline binningPipeline{VK_NULL_HANDLE};

        std::unique_ptr<LveDescriptorPool> globalPool;
        std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;

        VkSampler sampler{VK_NULL_HANDLE};
        VkSampler nearestSampler = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<Buffer>> meshBuffers;
        std::vector<std::unique_ptr<Buffer>> compactMaterialBuffers;
        std::vector<std::unique_ptr<Buffer>> worldPositionBuffers;

        std::vector<std::unique_ptr<Buffer>> binningMetaBuffers;
        std::vector<std::unique_ptr<Buffer>> pixelCoordBuffers;

        uint32_t lastWidth = 0;
        uint32_t lastHeight = 0;

        struct FrameCache {
            VkImageView depthView = VK_NULL_HANDLE;
            VkImageView visView = VK_NULL_HANDLE;
        };
        std::vector<FrameCache> frameCaches;
    };
}
