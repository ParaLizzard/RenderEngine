#pragma once

#include "Core/Device.h"
#include "Scene/Model.h"
#include "Renderer/RenderPassNode.h"
#include "Renderer/Swapchain.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>

#include "Renderer/ResourceHeap.h"

namespace Engine
{
    class Renderer;

    struct PrepassPushConstants
    {
        glm::mat4 viewProjection{1.0f};
        uint32_t debugIsTransparent;
    };

    struct DrawBatch
    {
        VkPipeline pipeline;
        uint32_t firstCommandOffset;
        uint32_t commandCount;
        uint32_t isTransparent;
        uint32_t isDoubleSided;
    };

    struct ObjectData {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 boundingSphere;
    };

    struct ComputePushConstants {
        glm::mat4 viewProjection;
        uint32_t objectCount;
    };

    class LightPassNode : public RenderPassNode
    {
    public:
        LightPassNode(Device& device, Renderer& renderer, Model& megaBuffer, ResourceHeap& resourceHeap);
        ~LightPassNode() override;

        LightPassNode(const LightPassNode&) = delete;
        LightPassNode& operator=(const LightPassNode&) = delete;

        void setup(RenderGraphBuilder& renderGraph) override;
        void execute(VkCommandBuffer& cmd, FrameInfo& frameInfo) override;

        Buffer* getGpuIndirectBuffer(uint32_t frameIndex) { return gpuIndirectCommandBuffers[frameIndex].get(); }
        VkDescriptorSet getObjectDescriptorSet(uint32_t frameIndex) { return objectDescriptorSets[frameIndex]; }
        const std::vector<DrawBatch>& getDrawBatches() { return drawBatches; }
        VkDescriptorSetLayout getObjectSetLayout() const { return objectSetLayout; }

        void markSceneDirty() { sceneDirty = true; }

    private:
        void createPipelineLayout();
        void createPipeline();
        void createComputePipeline();

        //VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        Device& device;
        Renderer& renderer;
        Model& megaBuffer;
        ResourceHeap& resourceHeap;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
        VkPipeline transparentPipeline = VK_NULL_HANDLE;
        VkPipeline graphicsPipelineDoubleSided = VK_NULL_HANDLE;
        VkPipeline transparentPipelineDoubleSided = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<Buffer>> cpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> gpuObjectSSBOs;
        std::vector<std::unique_ptr<Buffer>> cpuIndirectCommandBuffers;
        std::vector<std::unique_ptr<Buffer>> gpuIndirectCommandBuffers;

        VkDescriptorSetLayout objectSetLayout{VK_NULL_HANDLE};
        VkDescriptorPool objectDescriptorPool{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> objectDescriptorSets;

        VkPipelineLayout computePipelineLayout{VK_NULL_HANDLE};
        VkPipeline computePipeline{VK_NULL_HANDLE};

        std::vector<const GameObject*> opaqueDraws;
        std::vector<const GameObject*> transparentDraws;
        std::vector<ObjectData> objectDataArray;
        std::vector<VkDrawIndexedIndirectCommand> indirectCommandsArray;
        std::vector<DrawBatch> drawBatches;



        bool sceneDirty = true;
        int framesToUpdate = 0;
    };
}
