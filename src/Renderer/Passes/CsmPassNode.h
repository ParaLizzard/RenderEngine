#pragma once
#include "Renderer/RenderPassNode.h"
#include "Renderer/Passes/CullPassNode.h"

namespace Engine {

    class CsmPassNode : public RenderPassNode
    {
    public:
        CsmPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap, CullPassNode &cullPass);
        ~CsmPassNode() override;

        CsmPassNode(const CsmPassNode &) = delete;
        CsmPassNode &operator=(const CsmPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;

        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        void markSceneDirty() override
        {
            descriptorsUpdated = false;
        }

        void updateCascades(SceneUbo &sceneUbo, FrameInfo &frameInfo);
        [[nodiscard]] VkImageView getCsmArrayView() const { return csmArrayView; }

    private:

        struct CsmCullPushConstants
        {
            uint32_t objectCount;
            uint32_t objectCapacity;
        };

        struct CascadeGpuData
        {
            glm::mat4 viewProj[SHADOW_MAP_CASCADES];
            glm::vec4 frustumPlanes[SHADOW_MAP_CASCADES * 6];
        };

        void createPipelineLayout();
        void createPipeline();
        void updateDescriptors();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        ResourceHeap &resourceHeap;
        CullPassNode &cullPass;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

        VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
        VkPipeline computePipeline = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> objectDescriptorSets;
        VkDescriptorSetLayout objectSetLayout {VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool {VK_NULL_HANDLE};

        std::vector<std::unique_ptr<Buffer>> gpuCompactedIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuDrawCountBuffers;
        std::vector<std::unique_ptr<Buffer>> cascadeDataBuffers;

        glm::mat4 cascadeViewProjs[SHADOW_MAP_CASCADES];

        VkImage csmImageCache = VK_NULL_HANDLE;
        VkImageView csmArrayView = VK_NULL_HANDLE;

        bool descriptorsUpdated = false;
    };

} // namespace Engine