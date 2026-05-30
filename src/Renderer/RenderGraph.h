#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include "RenderPassNode.h"

namespace Engine
{
    // -------------------------------------------------------------------------
    // Image resource tracking
    // -------------------------------------------------------------------------

    struct GraphImage
    {
        VkImage               image       = VK_NULL_HANDLE;
        VkImageView           imageView   = VK_NULL_HANDLE;
        VkFormat              imageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D            extent      = {};
        VkImageLayout         layout      = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags2 lastStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        VkAccessFlags2        lastAccessMask = VK_ACCESS_2_NONE;
        uint32_t mipLevels   = 1;
        uint32_t arrayLayers = 1;
    };

    struct ImageUsageDeclaration
    {
        std::string           imageName;
        VkImageLayout         imageLayout;
        VkPipelineStageFlags2 stageMask;
        VkAccessFlags2        accessMask;
    };

    struct TransientImageDeclaration
    {
        std::string name;
        VkFormat format;
        VkExtent2D extent;
    };

    struct TransientResource
    {
        std::string name;
        VkImage image;
        VkImageView view;
        VmaAllocation allocation;
    };

    // -------------------------------------------------------------------------
    // Buffer resource tracking
    // -------------------------------------------------------------------------

    struct GraphBuffer
    {
        VkBuffer              buffer         = VK_NULL_HANDLE;
        VkDeviceSize          size           = 0;
        VkPipelineStageFlags2 lastStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        VkAccessFlags2        lastAccessMask = VK_ACCESS_2_NONE;
    };

    struct BufferUsageDeclaration
    {
        std::string           bufferName;
        VkPipelineStageFlags2 stageMask;
        VkAccessFlags2        accessMask;
    };

    // -------------------------------------------------------------------------
    // Pass execution info — carries both image and buffer usage declarations
    // -------------------------------------------------------------------------

    struct PassExecutionInfo
    {
        RenderPassNode*                      passNode = nullptr;
        std::vector<TransientImageDeclaration> transientImages;
        std::vector<ImageUsageDeclaration>   imageUsages;
        std::vector<BufferUsageDeclaration>  bufferUsages;
    };

    // -------------------------------------------------------------------------
    // RenderGraph
    // -------------------------------------------------------------------------

    class RenderGraph
    {
    public:
        RenderGraph(Device& device);

        void addPass(RenderPassNode* pass);

        void registerPhysicalImage(
            const std::string& name,
            VkImage image, VkImageView view,
            VkFormat format, VkExtent2D extent,
            VkImageLayout initialLayout);

        void registerPhysicalBuffer(
            const std::string& name,
            VkBuffer buffer,
            VkDeviceSize size,
            VkPipelineStageFlags2 initialStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VkAccessFlags2        initialAccessMask = VK_ACCESS_2_NONE);

        void compile();
        void execute(VkCommandBuffer cmdBuffer, FrameInfo& frameInfo);
        void clear();

        void transitionToPresent(VkCommandBuffer cmdBuffer, const std::string& imageName);

        bool isDepthFormat(VkFormat format);

    private:
        Device& device;

        std::vector<PassExecutionInfo>              registeredPasses;
        std::unordered_map<std::string, GraphImage>  imageRegistry;
        std::vector<TransientResource> transientRegistry;
        std::unordered_map<std::string, GraphBuffer> bufferRegistry;
    };

    // -------------------------------------------------------------------------
    // RenderGraphBuilder — used inside RenderPassNode::setup()
    // -------------------------------------------------------------------------

    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder(
            std::vector<ImageUsageDeclaration>&  imageUsagesList,
            std::vector<TransientImageDeclaration>&  transientImagesUsagesList,
            std::vector<BufferUsageDeclaration>& bufferUsagesList
            );

        // Image declarations
        void readImage(
            const std::string& name,
            VkImageLayout imageLayout,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask);

        void writeImage(
            const std::string& name,
            VkImageLayout imageLayout,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask);

        // Buffer declarations
        void readBuffer(
            const std::string& name,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask);

        void writeBuffer(
            const std::string& name,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask);

        void readWriteBuffer(
            const std::string& name,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask);

        void createTransientImage(const std::string& name, VkFormat format, VkExtent2D extent);

    private:
        std::vector<ImageUsageDeclaration>& imageUsages;
        std::vector<TransientImageDeclaration>& transientImageUsages;
        std::vector<BufferUsageDeclaration>& bufferUsages;
    };

} // namespace Engine