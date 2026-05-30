#include "RenderGraph.h"

namespace Engine
{
    RenderGraph::RenderGraph(Device& device) : device(device)
    {
    }

    void RenderGraph::addPass(RenderPassNode* pass)
    {
        PassExecutionInfo info{};
        info.passNode = pass;

        RenderGraphBuilder builder{info.imageUsages, info.transientImages, info.bufferUsages};
        pass->setup(builder);

        registeredPasses.push_back(std::move(info));
    }

    void RenderGraph::registerPhysicalImage(
        const std::string& name,
        VkImage image, VkImageView view,
        VkFormat format, VkExtent2D extent,
        VkImageLayout initialLayout)
    {
        GraphImage g{};
        g.image = image;
        g.imageView = view;
        g.imageFormat = format;
        g.extent = extent;
        g.layout = initialLayout;
        g.lastAccessMask = VK_ACCESS_2_NONE;
        g.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;

        imageRegistry[name] = g;
    }

    void RenderGraph::registerPhysicalBuffer(
        const std::string& name,
        VkBuffer buffer,
        VkDeviceSize size,
        VkPipelineStageFlags2 initialStageMask,
        VkAccessFlags2 initialAccessMask)
    {
        GraphBuffer g{};
        g.buffer = buffer;
        g.size = size;
        g.lastStageMask = initialStageMask;
        g.lastAccessMask = initialAccessMask;

        bufferRegistry[name] = g;
    }

    void RenderGraph::compile()
    {
        for (const PassExecutionInfo& pass : registeredPasses)
        {
            for (const TransientImageDeclaration& decl : pass.transientImages)
            {
                if (imageRegistry.find(decl.name) != imageRegistry.end()) continue;

                VkImage transientImage;
                VmaAllocation allocation;

                VkImageCreateInfo imageInfo{};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = decl.format;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                imageInfo.extent = VkExtent3D(decl.extent.width,decl.extent.height, 0.f);
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.pNext = nullptr;

                device.createImageWithInfo(imageInfo, transientImage, allocation);

                VkImageView transientImageView;
                VkImageViewCreateInfo imageViewInfo{};
                imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewInfo.image = transientImage;
                imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                imageViewInfo.format = decl.format;
                imageViewInfo.pNext = nullptr;
                imageViewInfo.subresourceRange.aspectMask = isDepthFormat(decl.format) ?
                                            VK_IMAGE_ASPECT_DEPTH_BIT :
                                            VK_IMAGE_ASPECT_COLOR_BIT;

                vkCreateImageView(device.getDevice(), {}, nullptr, &transientImageView);

                registerPhysicalImage(decl.name, transientImage, transientImageView, decl.format, decl.extent,
                                      VK_IMAGE_LAYOUT_UNDEFINED);

                transientRegistry.push_back({decl.name, transientImage, transientImageView, allocation});

            }

            for (const ImageUsageDeclaration& image : pass.imageUsages)
            {
                if (imageRegistry.find(image.imageName) == imageRegistry.end())
                    throw std::runtime_error("RenderGraph: Image '" + image.imageName + "' not registered");

                if (imageRegistry.at(image.imageName).image == VK_NULL_HANDLE)
                    throw std::runtime_error("RenderGraph: Image '" + image.imageName + "' is VK_NULL_HANDLE");
            }

            for (const BufferUsageDeclaration& buf : pass.bufferUsages)
            {
                if (bufferRegistry.find(buf.bufferName) == bufferRegistry.end())
                    throw std::runtime_error("RenderGraph: Buffer '" + buf.bufferName + "' not registered");

                if (bufferRegistry.at(buf.bufferName).buffer == VK_NULL_HANDLE)
                    throw std::runtime_error("RenderGraph: Buffer '" + buf.bufferName + "' is VK_NULL_HANDLE");
            }
        }
    }

    void RenderGraph::execute(VkCommandBuffer cmdBuffer, FrameInfo& frameInfo)
    {
        for (PassExecutionInfo& pass : registeredPasses)
        {
            std::vector<VkImageMemoryBarrier2> imageBarriers;
            std::vector<VkBufferMemoryBarrier2> bufferBarriers;

            imageBarriers.reserve(pass.imageUsages.size());
            bufferBarriers.reserve(pass.bufferUsages.size());

            for (ImageUsageDeclaration& decl : pass.imageUsages)
            {
                GraphImage& g = imageRegistry[decl.imageName];

                const bool layoutChanged = g.layout != decl.imageLayout;
                const bool accessChanged = g.lastAccessMask != decl.accessMask
                    || g.lastStageMask != decl.stageMask;

                if (layoutChanged || accessChanged)
                {
                    VkImageMemoryBarrier2 barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                    barrier.image = g.image;
                    barrier.oldLayout = g.layout;
                    barrier.newLayout = decl.imageLayout;
                    barrier.srcStageMask = g.lastStageMask;
                    barrier.srcAccessMask = g.lastAccessMask;
                    barrier.dstStageMask = decl.stageMask;
                    barrier.dstAccessMask = decl.accessMask;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                    VkImageSubresourceRange range{};
                    range.aspectMask = isDepthFormat(g.imageFormat)
                                           ? VK_IMAGE_ASPECT_DEPTH_BIT
                                           : VK_IMAGE_ASPECT_COLOR_BIT;
                    range.baseMipLevel = 0;
                    range.levelCount = g.mipLevels;
                    range.baseArrayLayer = 0;
                    range.layerCount = g.arrayLayers;
                    barrier.subresourceRange = range;

                    imageBarriers.push_back(barrier);
                }

                g.layout = decl.imageLayout;
                g.lastStageMask = decl.stageMask;
                g.lastAccessMask = decl.accessMask;
            }

            for (BufferUsageDeclaration& decl : pass.bufferUsages)
            {
                GraphBuffer& g = bufferRegistry[decl.bufferName];

                const bool accessChanged = g.lastAccessMask != decl.accessMask
                    || g.lastStageMask != decl.stageMask;

                if (accessChanged)
                {
                    VkBufferMemoryBarrier2 barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                    barrier.buffer = g.buffer;
                    barrier.offset = 0;
                    barrier.size = VK_WHOLE_SIZE;
                    barrier.srcStageMask = g.lastStageMask;
                    barrier.srcAccessMask = g.lastAccessMask;
                    barrier.dstStageMask = decl.stageMask;
                    barrier.dstAccessMask = decl.accessMask;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                    bufferBarriers.push_back(barrier);
                }

                g.lastStageMask = decl.stageMask;
                g.lastAccessMask = decl.accessMask;
            }

            // -----------------------------------------------------------------
            // Single vkCmdPipelineBarrier2 covering both images and buffers
            // -----------------------------------------------------------------
            if (!imageBarriers.empty() || !bufferBarriers.empty())
            {
                VkDependencyInfo dep{};
                dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dep.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
                dep.pImageMemoryBarriers = imageBarriers.data();
                dep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
                dep.pBufferMemoryBarriers = bufferBarriers.data();

                vkCmdPipelineBarrier2(cmdBuffer, &dep);
            }

            pass.passNode->execute(cmdBuffer, frameInfo);
        }
    }

    void RenderGraph::clear()
    {
        for (const TransientResource& res : transientRegistry)
        {
            vkDestroyImageView(device.getDevice(), res.view, nullptr);
            vmaDestroyImage(device.getAllocator(), res.image, res.allocation);
        }
        transientRegistry.clear();
        registeredPasses.clear();
        imageRegistry.clear();
        bufferRegistry.clear();
    }

    void RenderGraph::transitionToPresent(VkCommandBuffer cmdBuffer, const std::string& imageName)
    {
        auto it = imageRegistry.find(imageName);
        if (it == imageRegistry.end()) return;

        GraphImage& g = it->second;
        if (g.layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) return;

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image = g.image;
        barrier.oldLayout = g.layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcStageMask = g.lastStageMask;
        barrier.srcAccessMask = g.lastAccessMask;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_NONE;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = g.mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = g.arrayLayers;

        VkDependencyInfo dep{};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmdBuffer, &dep);

        g.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        g.lastStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        g.lastAccessMask = VK_ACCESS_2_NONE;
    }

    bool RenderGraph::isDepthFormat(VkFormat format)
    {
        switch (format)
        {
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

    RenderGraphBuilder::RenderGraphBuilder(
        std::vector<ImageUsageDeclaration>& imageUsagesList,
        std::vector<TransientImageDeclaration>& transientImageUsagesList,
        std::vector<BufferUsageDeclaration>& bufferUsagesList)
        : imageUsages(imageUsagesList), bufferUsages(bufferUsagesList), transientImageUsages(transientImageUsagesList)
    {
    }

    void RenderGraphBuilder::readImage(
        const std::string& name,
        VkImageLayout imageLayout,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        imageUsages.push_back({name, imageLayout, stageMask, accessMask});
    }

    void RenderGraphBuilder::writeImage(
        const std::string& name,
        VkImageLayout imageLayout,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        imageUsages.push_back({name, imageLayout, stageMask, accessMask});
    }

    void RenderGraphBuilder::readBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask});
    }

    void RenderGraphBuilder::writeBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask});
    }

    void RenderGraphBuilder::readWriteBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask});
    }

    void RenderGraphBuilder::createTransientImage(const std::string& name, VkFormat format, VkExtent2D extent)
    {
        transientImageUsages.push_back({name, format, extent});
    }
}
