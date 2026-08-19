#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "Vulkan/Buffer.h"
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

    private:

        struct CsmMeshPushConstants
        {
            uint32_t cascadeIndex;
            uint32_t maxTaskWgsPerCascade;
        };

        struct CsmCullPushConstants
        {
            uint32_t objectCount;
            uint32_t actualObjectCount;
            uint32_t cullFlags;
            uint32_t maxTaskWgsPerCascade;
        };

        struct CascadeGpuData
        {
            glm::mat4 viewProj[SHADOW_MAP_CASCADES];
            glm::vec4 frustumPlanes[SHADOW_MAP_CASCADES * 6];
            glm::vec4 cameraForward;
        };

        void createPipelineLayout();
        void createPipeline();
        void createMeshPipeline();
        void createMaskedMeshPipeline();
        void createMaskedPipeline();
        void updateDescriptors();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        ResourceHeap &resourceHeap;
        CullPassNode &cullPass;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

        VkPipelineLayout meshPipelineLayout = VK_NULL_HANDLE;
        VkPipeline meshPipeline = VK_NULL_HANDLE;

        VkPipelineLayout maskedPipelineLayout = VK_NULL_HANDLE;
        VkPipeline maskedPipeline = VK_NULL_HANDLE;
        VkPipeline maskedMeshPipeline = VK_NULL_HANDLE;

        VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
        VkPipeline objectCullPipeline = VK_NULL_HANDLE;
        VkPipeline taskSubmitPipeline = VK_NULL_HANDLE;
        VkPipeline meshletCullPipeline = VK_NULL_HANDLE;
        VkPipeline triangleCullPipeline = VK_NULL_HANDLE;

        VkDescriptorSetLayout objectSetLayout {VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool {VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> objectDescriptorSets;

        std::vector<std::unique_ptr<Buffer>> gpuDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuVisibleObjectBuffers;
        std::vector<std::unique_ptr<Buffer>> singleIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> compactedIndexBuffers;
        std::vector<std::unique_ptr<Buffer>> visibleMeshletBuffers;
        std::vector<std::unique_ptr<Buffer>> triangleDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> taskWorkgroupBuffers;
        std::vector<std::unique_ptr<Buffer>> taskDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedTaskWorkgroupBuffers;
        std::vector<std::unique_ptr<Buffer>> maskedTaskDispatchCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> cascadeDataBuffers;

        glm::mat4 cascadeViewProjs[SHADOW_MAP_CASCADES];
        PFN_vkCmdDrawMeshTasksIndirectEXT pfn_vkCmdDrawMeshTasksIndirectEXT {nullptr};
        bool descriptorsUpdated = false;
    };

} // namespace Engine