#include "RenderGraph.h"

#include <stdexcept>
#include <string>

namespace Engine
{
    namespace
    {
        void vkCheck(VkResult result, const char* what)
        {
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error(std::string("RenderGraph: ") + what + " failed with VkResult " + std::to_string(result));
            }
        }
    }

    RenderGraph::RenderGraph(Device& device) : device(device)
    {
    }

    RenderGraph::~RenderGraph()
    {
        for (auto& pair : transientCache)
        {
            destroyTransientResource(pair.second);
        }

        transientCache.clear();
        registeredPasses.clear();
        imageRegistry.clear();
        bufferRegistry.clear();
    }

    void RenderGraph::destroyTransientResource(TransientResource& resource)
    {
        if (resource.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device.getDevice(), resource.view, nullptr);
            resource.view = VK_NULL_HANDLE;
        }
        if (resource.image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(device.getAllocator(), resource.image, resource.allocation);
            resource.image = VK_NULL_HANDLE;
            resource.allocation = nullptr;
        }
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
        VkImageLayout initialLayout,
        VkPipelineStageFlags2 initialStageMask,
        VkAccessFlags2 initialAccessMask)
    {
        GraphImage g{};
        g.image = image;
        g.imageView = view;
        g.imageFormat = format;
        g.extent = extent;
        g.layout = initialLayout;
        g.lastAccessMask = initialAccessMask;
        g.lastStageMask = initialStageMask;
        g.mipLevels = 1;
        g.arrayLayers = 1;

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

                auto cacheIt = transientCache.find(decl.name);
                if (cacheIt != transientCache.end())
                {
                    TransientResource& cached = cacheIt->second;

                    if (cached.extent.width != decl.extent.width || cached.extent.height != decl.extent.height)
                    {
                        destroyTransientResource(cached);
                        transientCache.erase(cacheIt);
                    }
                    else
                    {
                        registerPhysicalImage(decl.name, cached.image, cached.view, decl.format, decl.extent,
                                               cached.lastLayout, cached.lastStageMask, cached.lastAccessMask);
                        continue;
                    }
                }

                VkImage transientImage;
                VmaAllocation allocation;

                bool isDepth = isDepthFormat(decl.format);

                VkImageCreateInfo imageInfo{};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = decl.format;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.usage = decl.usage;
                imageInfo.extent = {decl.extent.width, decl.extent.height, 1};
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
                imageViewInfo.subresourceRange.aspectMask = isDepth ?
                                            VK_IMAGE_ASPECT_DEPTH_BIT :
                                            VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewInfo.subresourceRange.levelCount = 1;
                imageViewInfo.subresourceRange.layerCount = 1;

                vkCheck(vkCreateImageView(device.getDevice(), &imageViewInfo, nullptr, &transientImageView), "vkCreateImageView (transient)");

                TransientResource res{};
                res.name = decl.name;
                res.image = transientImage;
                res.view = transientImageView;
                res.allocation = allocation;
                res.extent = decl.extent;
                res.lastLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                res.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                res.lastAccessMask = VK_ACCESS_2_NONE;

                transientCache[decl.name] = res;

                registerPhysicalImage(decl.name, transientImage, transientImageView, decl.format, decl.extent, VK_IMAGE_LAYOUT_UNDEFINED);
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
            pass.passNode->resolve(*this, frameInfo);
        }

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

        for (auto& pair : transientCache)
        {
            auto it = imageRegistry.find(pair.first);
            if (it == imageRegistry.end()) continue;

            pair.second.lastLayout = it->second.layout;
            pair.second.lastStageMask = it->second.lastStageMask;
            pair.second.lastAccessMask = it->second.lastAccessMask;
        }
    }

    void RenderGraph::clear()
    {
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

        auto cacheIt = transientCache.find(imageName);
        if (cacheIt != transientCache.end())
        {
            cacheIt->second.lastLayout = g.layout;
            cacheIt->second.lastStageMask = g.lastStageMask;
            cacheIt->second.lastAccessMask = g.lastAccessMask;
        }
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

    VkImageView RenderGraph::getImageView(const std::string& name) const
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end())
        {
            return it->second.imageView;
        }
        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered image: " + name);
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
        imageUsages.push_back({name, imageLayout, stageMask, accessMask, ResourceUsageType::Read});
    }

    void RenderGraphBuilder::writeImage(
        const std::string& name,
        VkImageLayout imageLayout,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        imageUsages.push_back({name, imageLayout, stageMask, accessMask, ResourceUsageType::Write});
    }

    void RenderGraphBuilder::readBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::Read});
    }

    void RenderGraphBuilder::writeBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::Write});
    }

    void RenderGraphBuilder::readWriteBuffer(
        const std::string& name,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::ReadWrite});
    }

    void RenderGraphBuilder::createTransientImage(
        const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkClearValue clearValue)
    {
        transientImageUsages.push_back({name, format, extent, usage, clearValue});
    }

    void RenderGraph::updateImageHandle(const std::string& name, VkImage image, VkImageView view, VkExtent2D extent)
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end())
        {
            if (it->second.image != image)
            {
                it->second.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                it->second.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                it->second.lastAccessMask = VK_ACCESS_2_NONE;
            }

            it->second.image = image;
            it->second.imageView = view;
            it->second.extent = extent;
        }
    }

    void RenderGraph::updateBufferHandle(const std::string& name, VkBuffer buffer, VkDeviceSize size)
    {
        auto it = bufferRegistry.find(name);
        if (it != bufferRegistry.end())
        {
            it->second.buffer = buffer;
            it->second.size = size;
        }
    }

    VkImage RenderGraph::getImage(const std::string& name) const
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end())
        {
            return it->second.image;
        }
        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered image: " + name);
    }

    VkDescriptorBufferInfo RenderGraph::getBufferInfo(const std::string& name, int32_t currentFrame)
    {
        (void)currentFrame;

        auto it = bufferRegistry.find(name);
        if (it != bufferRegistry.end())
        {
            return VkDescriptorBufferInfo{it->second.buffer, 0, VK_WHOLE_SIZE};
        }

        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered buffer: " + name);
    }
}