#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/FrameInfo.h"
#include "Core/Descriptor.h"

#define SSAO_KERNEL_SIZE 64
#define SSAO_RADIUS 0.4f
#define SSAO_NOISE_DIM 4

namespace Engine
{
    class Renderer;

    struct SsaoUbo {
        glm::mat4 projection;
        glm::mat4 invProjection;
        glm::mat4 view;
        glm::vec4 samples[SSAO_KERNEL_SIZE];
        float nearPlane;
        float farPlane;
    };

    class SsaoPassNode : public RenderPassNode
    {
    public:
        SsaoPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap);
        ~SsaoPassNode() override;

        SsaoPassNode(const SsaoPassNode&) = delete;
        SsaoPassNode& operator=(const SsaoPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;
        void resolve(RenderGraph& graph, const FrameInfo& frameInfo) override;

    private:
        void createNoiseTexture();
        void createPipelines();

        Device& device;
        Renderer& renderer;
        Model& megaBuffer;
        ResourceHeap& resourceHeap;

        // Pipelines
        VkPipelineLayout ssaoPipelineLayout{VK_NULL_HANDLE};
        VkPipeline ssaoPipeline{VK_NULL_HANDLE};
        VkPipelineLayout blurPipelineLayout{VK_NULL_HANDLE};
        VkPipeline blurPipeline{VK_NULL_HANDLE};

        // Descriptors
        std::unique_ptr<LveDescriptorPool> descriptorPool;

        std::unique_ptr<LveDescriptorSetLayout> ssaoSetLayout;
        std::unique_ptr<LveDescriptorSetLayout> blurSetLayout;

        std::vector<VkDescriptorSet> ssaoDescriptorSets;
        std::vector<VkDescriptorSet> blurDescriptorSets;

        std::vector<std::unique_ptr<Buffer>> uboBuffers;

        std::vector<glm::vec4> ssaoKernel;

        // Noise Texture
        VkImage noiseImage{VK_NULL_HANDLE};
        VmaAllocation noiseAllocation{VK_NULL_HANDLE};
        VkImageView noiseView{VK_NULL_HANDLE};
        VkSampler noiseSampler{VK_NULL_HANDLE};
        VkSampler colorSampler{VK_NULL_HANDLE};
    };
}