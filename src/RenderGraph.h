#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "RenderPassNode.h"

namespace Engine
{
    struct GraphImage
    {
        VkImage image;
        VkImageView imageView;
        VkFormat imageFormat;
        VkExtent2D extent;
        VkImageLayout layout;
        VkPipelineStageFlags2 lastStageMask;
        VkAccessFlags2 lastAccessMask;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
    };

    struct ImageUsageDeclaration
    {
        std::string imageName;
        VkImageLayout imageLayout;
        VkPipelineStageFlags2 stageMask;
        VkAccessFlags2 accessMask;
    };

    struct PassExecutionInfo
    {
        RenderPassNode* passNode;
        std::vector<ImageUsageDeclaration> usages;
    };

    class RenderGraph
    {
    public:
        void addPass(std::unique_ptr<RenderPassNode> pass);
        void registerPhysicalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent, VkImageLayout initialLayout);
        void compile();
        void execute(VkCommandBuffer cmdBuffer, FrameInfo& frameInfo);
        void clear();
        bool isDepthFormat(VkFormat format);

    private:
        std::vector<std::unique_ptr<RenderPassNode>> ownedPasses;
        std::vector<PassExecutionInfo> registeredPasses;
        std::unordered_map<std::string, GraphImage> resourceRegistry;
    };

    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder(std::vector<ImageUsageDeclaration>& usagesList);

        void readImage(const std::string& name, VkImageLayout imageLayout, VkPipelineStageFlags2 stageMask, VkAccessFlags2 accessMask);
        void writeImage(const std::string& name, VkImageLayout imageLayout, VkPipelineStageFlags2 stageMask, VkAccessFlags2 accessMask);
    private:
        std::vector<ImageUsageDeclaration>& usages;
    };

}