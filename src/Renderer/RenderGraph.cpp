#include "RenderGraph.h"

namespace Engine
{
    void RenderGraph::addPass(RenderPassNode* pass)
    {
        PassExecutionInfo info{};
        info.passNode = pass;

        RenderGraphBuilder builder{info.usages};
        pass->setup(builder);

        registeredPasses.push_back(info);
    }

    void RenderGraph::registerPhysicalImage(const std::string& name, VkImage image, VkImageView view,
        VkFormat format, VkExtent2D extent, VkImageLayout initialLayout)
    {
        GraphImage graphImage{};
        graphImage.image = image;
        graphImage.imageView = view;
        graphImage.imageFormat = format;
        graphImage.extent = extent;
        graphImage.layout = initialLayout;
        graphImage.lastAccessMask = VK_ACCESS_2_NONE;
        graphImage.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;

        resourceRegistry[name] = graphImage;
    }

    void RenderGraph::compile()
    {
        for (PassExecutionInfo& pass : registeredPasses)
        {
            for (ImageUsageDeclaration image: pass.usages)
            {
                if (resourceRegistry.find(image.imageName) == resourceRegistry.end()) {
                    throw std::runtime_error("RenderGraph: Resource '" + image.imageName + "' not registered");
                }
                const auto& graphImage = resourceRegistry.at(image.imageName);
                if (graphImage.image == VK_NULL_HANDLE) {
                    throw std::runtime_error("RenderGraph: Resource '" + image.imageName + "' is VK_NULL_HANDLE");
                }
            }
        }
    }

    void RenderGraph::execute(VkCommandBuffer cmdBuffer, FrameInfo& frameInfo)
    {
        for (PassExecutionInfo& pass : registeredPasses)
        {
            std::vector<VkImageMemoryBarrier2> imageBarriers;
            imageBarriers.reserve(pass.usages.size());

            for (ImageUsageDeclaration& image: pass.usages)
            {
                GraphImage& graphImage = resourceRegistry[image.imageName];

                if (graphImage.layout != image.imageLayout)
                {


                    VkImageMemoryBarrier2 imageBarrier{};
                    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                    imageBarrier.oldLayout = graphImage.layout;
                    imageBarrier.newLayout = image.imageLayout;
                    imageBarrier.image = graphImage.image;
                    imageBarrier.pNext = nullptr;

                    VkImageSubresourceRange subresourceRange{};
                    if (isDepthFormat(graphImage.imageFormat))
                    {
                        subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    } else
                    {
                        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    }
                    subresourceRange.baseMipLevel = 0;
                    subresourceRange.levelCount = graphImage.mipLevels;
                    subresourceRange.layerCount = graphImage.arrayLayers;

                    imageBarrier.subresourceRange = subresourceRange;
                    imageBarrier.srcAccessMask = graphImage.lastAccessMask;
                    imageBarrier.dstAccessMask = image.accessMask;

                    imageBarrier.srcStageMask = graphImage.lastStageMask;
                    imageBarrier.dstStageMask = image.stageMask;

                    imageBarriers.push_back(imageBarrier);
                }

                graphImage.layout = image.imageLayout;

                graphImage.lastStageMask = image.stageMask;
                graphImage.lastAccessMask = image.accessMask;
            }

            if (!imageBarriers.empty())
            {
                VkDependencyInfo dependencyInfo{
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext = nullptr,
                    .dependencyFlags = 0,
                    .memoryBarrierCount = 0,
                    .pMemoryBarriers = nullptr,
                    .bufferMemoryBarrierCount = 0,
                    .pBufferMemoryBarriers = nullptr,
                    .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size()),
                    .pImageMemoryBarriers = imageBarriers.data(),
                };

                vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            }

            pass.passNode->execute(cmdBuffer, frameInfo);
        }
    }

    void RenderGraph::clear()
    {
        registeredPasses.clear();
        resourceRegistry.clear();
    }

    RenderGraphBuilder::RenderGraphBuilder(std::vector<ImageUsageDeclaration>& usagesList) : usages(usagesList)
    {
    }

    void RenderGraphBuilder::readImage(const std::string& name, VkImageLayout imageLayout, VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        ImageUsageDeclaration declaration{};
        declaration.imageName = name;
        declaration.imageLayout = imageLayout;
        declaration.stageMask = stageMask;
        declaration.accessMask = accessMask;

        usages.push_back(declaration);
    }

    void RenderGraphBuilder::writeImage(const std::string& name, VkImageLayout imageLayout, VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        ImageUsageDeclaration declaration{};
        declaration.imageName = name;
        declaration.imageLayout = imageLayout;
        declaration.stageMask = stageMask;
        declaration.accessMask = accessMask;

        usages.push_back(declaration);
    }

    void RenderGraph::transitionToPresent(VkCommandBuffer cmdBuffer, const std::string& imageName)
    {
        if (resourceRegistry.find(imageName) == resourceRegistry.end()) return;

        GraphImage& graphImage = resourceRegistry[imageName];

        if (graphImage.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            VkImageMemoryBarrier2 imageBarrier{};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.oldLayout = graphImage.layout;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.image = graphImage.image;
            imageBarrier.pNext = nullptr;

            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange.baseMipLevel = 0;
            imageBarrier.subresourceRange.levelCount = graphImage.mipLevels;
            imageBarrier.subresourceRange.layerCount = graphImage.arrayLayers;

            imageBarrier.srcStageMask = graphImage.lastStageMask;
            imageBarrier.srcAccessMask = graphImage.lastAccessMask;

            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_NONE;

            VkDependencyInfo dependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &imageBarrier,
            };

            vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);

            graphImage.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            graphImage.lastStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            graphImage.lastAccessMask = VK_ACCESS_2_NONE;
        }
    }

    bool RenderGraph::isDepthFormat(VkFormat format) {
        switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }
}