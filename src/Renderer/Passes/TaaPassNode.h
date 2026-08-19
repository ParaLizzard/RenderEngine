#pragma once
#include "Renderer/RenderGraph.h"
#include "Vulkan/ResourceHeap.h"
#include "Renderer/FrameInfo.h"
#include "Vulkan/Descriptor.h"
#include "vma/vk_mem_alloc.h"

namespace Engine {
    class Renderer;
    class Model;

    class TaaPassNode: public RenderPassNode
    {
    public:
        TaaPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~TaaPassNode() override;

        TaaPassNode(const TaaPassNode &) = delete;
        TaaPassNode &operator=(const TaaPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void execute(VkCommandBuffer&cmd, FrameInfo &frameInfo) override;
        void registerResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void updateResources(RenderGraph &graph, const FrameInfo &frameInfo) override;

    private:
        void createPipelineLayout();
        void createPipeline();

        void createHistoryResources();
        void destroyHistoryResources();
        

    private:
        Device &device;
        Renderer &renderer;
        Model &megabBuffer;
        ResourceHeap &resourceHeap;

        std::unique_ptr<DescriptorPool> descriptorPool;
        std::unique_ptr<DescriptorSetLayout> setLayout;

        std::vector<VkDescriptorSet> descriptorSets;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;

        VkSampler linearSampler;
        VkSampler nearestSampler;

        VkExtent2D extent{0,0};
        bool historyflag = true;
        uint32_t historyPingPong = 0;

        struct HistoryImage
        {
            VkImage image = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
        } historyBuffers[2], velocityHistoryBuffers[2];
    };

} // namespace Engine

