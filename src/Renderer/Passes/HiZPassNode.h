#pragma once
#include "Renderer/RenderPassNode.h"
#include "Vulkan/Descriptor.h"
#include "Vulkan/Buffer.h"
#include "vma/vk_mem_alloc.h"
#include "Core/EngineConstants.h"

namespace Engine
{
    class HiZPassNode: public RenderPassNode
    {
    public:
        static constexpr uint32_t WORKGROUP_SIZE = Constants::HIZ_MIP_GENERATION_WORKGROUP_SIZE;

        HiZPassNode(Device& device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~HiZPassNode();

        HiZPassNode(const HiZPassNode &) = delete;
        HiZPassNode &operator=(const HiZPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void registerResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void updateResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void execute(VkCommandBuffer&cmd, FrameInfo &frameInfo) override;

    private:
        void createPipelineLayout();
        void createPipeline();

        void createHiZResources(VkExtent2D depthExtent);
        void destroyHiZResources();

    private:
        Device& device;
        Renderer &renderer;
        Model &megaBuffer;
        ResourceHeap &resourceHeap;

        VkImage HiZImage = VK_NULL_HANDLE;
        VkImageView HiZImageView = VK_NULL_HANDLE;
        VmaAllocation HiZMemory = VK_NULL_HANDLE;

        std::unique_ptr<Buffer> atomicCounterBuffer;
        std::unique_ptr<DescriptorPool> descriptorPool;
        std::unique_ptr<DescriptorSetLayout> setLayout;
        std::vector<VkDescriptorSet> descriptorSets;

        VkSampler nearestSampler;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;

        VkExtent2D hizExtent = {0,0};
        int32_t hizMipLevels = 0;
        std::vector<VkImageView> HizMipViews;

        struct HiZPushConstants
        {
            uint32_t mips;
            uint32_t numWorkGroups;
            uint32_t workGroupOffset[2];
        };
    private:

        inline VkExtent2D getHiZExtent(VkExtent2D depthExtent) {
            return {
                std::bit_ceil(depthExtent.width),   // e.g. 1920 -> 2048
                std::bit_ceil(depthExtent.height)   // e.g. 1080 -> 2048 (or 1024)
            };
        }
        inline uint32_t getHiZMipLevels(VkExtent2D hizExtent) {
            uint32_t maxDim = std::max(hizExtent.width, hizExtent.height);
            return std::bit_width(maxDim); // e.g. 2048 -> 12 mips (2048 down to 1x1)
        }
    };
} // Engine
