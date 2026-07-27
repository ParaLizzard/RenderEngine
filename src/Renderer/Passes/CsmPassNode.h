#pragma once
#include "Renderer/RenderPassNode.h"

namespace Engine {

    class CsmPassNode : public RenderPassNode
    {
    public:
        CsmPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~CsmPassNode() override;

        CsmPassNode(const CsmPassNode &) = delete;
        CsmPassNode &operator=(const CsmPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void execute(VkCommandBuffer&cmd, FrameInfo &frameInfo) override;

        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        void markSceneDirty() override
        {
            sceneDirty = true;
        }

        void updateCascades(SceneUbo &sceneUbo, FrameInfo &frameInfo);

    private:



        struct ComputePushConstants
        {
            glm::mat4 viewProj;
            glm::vec4 frustumPlanes[6];
            glm::uint objectCount;
            glm::uint cascadeIndex;
            glm::uint objectCapacity;
            glm::uint clipPlaneCount;
        };

        struct CsmPassPushConstants
        {
            glm::uint cascadeIndex;
        };

        struct ObjectData
        {
            glm::mat4 modelMatrix;
            glm::mat4 normalMatrix;
            glm::vec4 boundingSphere;
        };

        void createPipelineLayout();
        void createPipeline();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        ResourceHeap &resourceHeap;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

        VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
        VkPipeline computePipeline = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> objectDescriptorSets;
        VkDescriptorSetLayout objectSetLayout {VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool {VK_NULL_HANDLE};

        std::vector<ObjectData> objectDataArray;
        std::vector<VkDrawIndexedIndirectCommand> indirectCommandsArray;
        std::vector<const GameObject *> opaqueDraws;

        std::vector<std::unique_ptr<Buffer>> cpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> gpuObjectSSBOs;

        std::vector<std::unique_ptr<Buffer>> cpuIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuIndirectCommandBuffers;

        std::vector<std::unique_ptr<Buffer>> gpuCompactedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers;

        bool sceneDirty = true;
        int framesToUpdate = 0;

        glm::mat4 cascadeViewProjs[SHADOW_MAP_CASCADES];

        VkImage csmImageCache = VK_NULL_HANDLE;
        VkImageView cascadeViews[SHADOW_MAP_CASCADES] = {VK_NULL_HANDLE};
    };

} // namespace Engine