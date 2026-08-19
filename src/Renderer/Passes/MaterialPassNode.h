#pragma once
#include "Renderer/RenderPassNode.h"
#include "Vulkan/Buffer.h"
#include <memory>
#include <vector>

namespace Engine {
    class Device;
    class Renderer;
    class Model;
    class ResourceHeap;
    class DescriptorPool;
    class DescriptorSetLayout;
    class Buffer;
    class CullPassNode;
    class CsmPassNode;
    struct FrameInfo;
    struct MaterialPushConstants
    {
        glm::mat4 viewProj;
        glm::mat4 view;
        glm::vec3 cameraPos;
        glm::uint enableSSAO;
        glm::uint debugMode;
        float ssaoStrength;
        glm::vec2 jitterOffset;
        glm::vec2 resolution;
        glm::vec2 rcpResolution;
    };

    class MaterialPassNode: public RenderPassNode
    {
    public:
        MaterialPassNode(Device &device,
                         Renderer &renderer,
                         Model &megaBuffer,
                         ResourceHeap &resourceHeap,
                         CullPassNode &cullPass,
                         RenderGraph &renderGraph);
        ~MaterialPassNode();

        MaterialPassNode(const MaterialPassNode &) = delete;
        MaterialPassNode &operator=(const MaterialPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void registerResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void updateResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        void markSceneDirty() override
        {
        }

    private:
        void createPipelineLayout();
        void createPipeline();

        Device &device;
        Model &megaBuffer;
        Renderer &renderer;
        ResourceHeap &resourceHeap;
        CullPassNode &cullPass;
        RenderGraph &renderGraph;

        VkPipelineLayout pipelineLayout {VK_NULL_HANDLE};
        VkPipeline pipeline {VK_NULL_HANDLE};

        std::unique_ptr<DescriptorPool> globalPool;
        std::unique_ptr<DescriptorSetLayout> globalSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;

        VkSampler sampler {VK_NULL_HANDLE};
        VkSampler nearestSampler = VK_NULL_HANDLE;
        VkSampler shadowSampler {VK_NULL_HANDLE};
        VkSampler hardwareShadowSampler {VK_NULL_HANDLE};
    };
} // namespace Engine

