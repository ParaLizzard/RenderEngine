#include "Renderer/RenderGraph.h"
#include "Core/EngineConfig.h"
#include "Vulkan/Device.h"
#include "Vulkan/VkUtils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Engine {
    RenderGraph::RenderGraph(Device &device): device(device)
    {
        startTime = std::chrono::high_resolution_clock::now();
    }

    RenderGraph::~RenderGraph()
    {
        if (profilerQueryPool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device.getDevice(), profilerQueryPool, nullptr);
            profilerQueryPool = VK_NULL_HANDLE;
        }

        for (auto &pair: transientCache) {
            vkDestroyImageView(device.getDevice(), pair.second.view, nullptr);
            vmaDestroyImage(device.getAllocator(), pair.second.image, pair.second.allocation);
        }

        transientCache.clear();
        registeredPasses.clear();
        imageRegistry.clear();
        bufferRegistry.clear();

        clear();
    }

    void RenderGraph::addPass(RenderPassNode *pass)
    {
        PassExecutionInfo info {};
        info.passNode = pass;

        RenderGraphBuilder builder {info.imageUsages, info.transientImages, info.bufferUsages};
        pass->setup(builder);

        registeredPasses.push_back(std::move(info));
    }

    void RenderGraph::registerPassResources(const FrameInfo &frameInfo)
    {
        for (const PassExecutionInfo &pass : registeredPasses) {
            pass.passNode->registerResources(*this, frameInfo);
        }
    }

    void RenderGraph::updatePassResources(const FrameInfo &frameInfo)
    {
        for (const PassExecutionInfo &pass : registeredPasses) {
            pass.passNode->updateResources(*this, frameInfo);
        }
    }

    void RenderGraph::registerPhysicalImage(const std::string &name,
                                            VkImage image,
                                            VkImageView view,
                                            VkFormat format,
                                            VkExtent2D extent,
                                            VkImageLayout initialLayout,
                                            uint32_t arrayLayers,
                                            uint32_t mipLevels)
    {
        GraphImage g {};
        g.image = image;
        g.imageView = view;
        g.imageFormat = format;
        g.extent = extent;
        g.layout = initialLayout;
        g.arrayLayers = arrayLayers;
        g.mipLevels = mipLevels;
        g.lastAccessMask = VK_ACCESS_2_NONE;
        g.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;

        imageRegistry[name] = g;
    }

    void RenderGraph::registerPhysicalBuffer(const std::string &name,
                                             VkBuffer buffer,
                                             VkDeviceSize size,
                                             VkPipelineStageFlags2 initialStageMask,
                                             VkAccessFlags2 initialAccessMask)
    {
        GraphBuffer g {};
        g.buffer = buffer;
        g.size = size;
        g.lastStageMask = initialStageMask;
        g.lastAccessMask = initialAccessMask;

        bufferRegistry[name] = g;
    }

    void RenderGraph::compile()
    {
        for (const PassExecutionInfo &pass: registeredPasses) {
            for (const TransientImageDeclaration &decl: pass.transientImages) {
                if (imageRegistry.find(decl.name) != imageRegistry.end())
                    continue;

                if (transientCache.find(decl.name) != transientCache.end()) {
                    TransientResource &cached = transientCache[decl.name];

                    if (cached.extent.width != decl.extent.width || cached.extent.height != decl.extent.height) {
                        vkDestroyImageView(device.getDevice(), cached.view, nullptr);
                        vmaDestroyImage(device.getAllocator(), cached.image, cached.allocation);
                        transientCache.erase(decl.name);
                    } else {
                        registerPhysicalImage(
                            decl.name, cached.image, cached.view, decl.format, decl.extent, VK_IMAGE_LAYOUT_UNDEFINED, decl.arrayLayers);
                        continue;
                    }
                }

                VkImage transientImage;
                VmaAllocation allocation;

                bool isDepth = VkUtils::isDepthFormat(decl.format);

                VkImageCreateInfo imageInfo {};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.format = decl.format;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = decl.arrayLayers;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.usage = decl.usage;
                imageInfo.extent = {decl.extent.width, decl.extent.height, 1};
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.pNext = nullptr;

                device.createImageWithInfo(imageInfo, transientImage, allocation);

                VkImageView transientImageView;
                VkImageViewCreateInfo imageViewInfo {};
                imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewInfo.image = transientImage;
                imageViewInfo.viewType = decl.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
                imageViewInfo.format = decl.format;
                imageViewInfo.pNext = nullptr;
                imageViewInfo.subresourceRange.aspectMask =
                    VkUtils::isDepthFormat(decl.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewInfo.subresourceRange.levelCount = 1;
                imageViewInfo.subresourceRange.layerCount = decl.arrayLayers;

                vkCreateImageView(device.getDevice(), &imageViewInfo, nullptr, &transientImageView);

                TransientResource res {};
                res.name = decl.name;
                res.image = transientImage;
                res.view = transientImageView;
                res.allocation = allocation;
                res.extent = decl.extent;

                transientCache[decl.name] = res;

                registerPhysicalImage(
                    decl.name, transientImage, transientImageView, decl.format, decl.extent, VK_IMAGE_LAYOUT_UNDEFINED, decl.arrayLayers);
            }

            for (const ImageUsageDeclaration &image: pass.imageUsages) {
                if (imageRegistry.find(image.imageName) == imageRegistry.end())
                    throw std::runtime_error("RenderGraph: Image '" + image.imageName + "' not registered");

                if (imageRegistry.at(image.imageName).image == VK_NULL_HANDLE)
                    throw std::runtime_error("RenderGraph: Image '" + image.imageName + "' is VK_NULL_HANDLE");
            }

            for (const BufferUsageDeclaration &buf: pass.bufferUsages) {
                if (bufferRegistry.find(buf.bufferName) == bufferRegistry.end())
                    throw std::runtime_error("RenderGraph: Buffer '" + buf.bufferName + "' not registered");

                if (bufferRegistry.at(buf.bufferName).buffer == VK_NULL_HANDLE)
                    throw std::runtime_error("RenderGraph: Buffer '" + buf.bufferName + "' is VK_NULL_HANDLE");
            }
        }
    }

    void RenderGraph::execute(VkCommandBuffer cmdBuffer, FrameInfo &frameInfo)
    {
        for (PassExecutionInfo &pass: registeredPasses) {
            pass.passNode->resolve(*this, frameInfo);
        }

        if (profilerQueryPool == VK_NULL_HANDLE) {
            VkQueryPoolCreateInfo queryPoolInfo {};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolInfo.queryCount = Config::MAX_FRAMES_IN_FLIGHT * 256;
            if (vkCreateQueryPool(device.getDevice(), &queryPoolInfo, nullptr, &profilerQueryPool) != VK_SUCCESS) {
                profilerQueryPool = VK_NULL_HANDLE;
            } else {

                vkResetQueryPool(device.getDevice(), profilerQueryPool, 0, Config::MAX_FRAMES_IN_FLIGHT * 256);
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSeconds = std::chrono::duration<double>(now - startTime).count();

        if (!profilingStarted && elapsedSeconds >= 30.0) {
            profilingStarted = true;
        }

        bool activeProfiling = profilingStarted && !profileSummaryPrinted && (profilerQueryPool != VK_NULL_HANDLE);
        
        static int framesProfiledCount = 0;
        if (activeProfiling) {
            framesProfiledCount++;
        }

        uint32_t passCount = static_cast<uint32_t>(registeredPasses.size());
        uint32_t currentFrame = frameInfo.frameIndex;
        uint32_t frameOffset = currentFrame * 256;
        currentFrameOffset = frameOffset;
        currentQueryCount = 0;
        currentProfileDepth = 0;
        activeMarkers.clear();

        if (profilerQueryPool != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(cmdBuffer, profilerQueryPool, frameOffset, 256);
        }

        for (size_t passIdx = 0; passIdx < registeredPasses.size(); ++passIdx) {
            PassExecutionInfo &pass = registeredPasses[passIdx];
            std::vector<VkImageMemoryBarrier2> imageBarriers;
            std::vector<VkBufferMemoryBarrier2> bufferBarriers;

            imageBarriers.reserve(pass.imageUsages.size());
            bufferBarriers.reserve(pass.bufferUsages.size());

            for (ImageUsageDeclaration &decl: pass.imageUsages) {
                GraphImage &g = imageRegistry[decl.imageName];

                const bool layoutChanged = g.layout != decl.imageLayout;
                const bool accessChanged = g.lastAccessMask != decl.accessMask || g.lastStageMask != decl.stageMask;

                if (layoutChanged || accessChanged) {
                    VkImageSubresourceRange range {};
                    range.aspectMask =
                        VkUtils::isDepthFormat(g.imageFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                    range.baseMipLevel = 0;
                    range.levelCount = g.mipLevels;
                    range.baseArrayLayer = 0;
                    range.layerCount = g.arrayLayers;

                    VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
                        g.image, g.layout, decl.imageLayout,
                        g.lastStageMask, g.lastAccessMask,
                        decl.stageMask, decl.accessMask, range);

                    imageBarriers.push_back(barrier);
                }

                g.layout = decl.imageLayout;
                g.lastStageMask = decl.stageMask;
                g.lastAccessMask = decl.accessMask;
            }

            for (BufferUsageDeclaration &decl: pass.bufferUsages) {
                GraphBuffer &g = bufferRegistry[decl.bufferName];

                const bool accessChanged = g.lastAccessMask != decl.accessMask || g.lastStageMask != decl.stageMask;

                if (accessChanged) {
                    VkBufferMemoryBarrier2 barrier = VkUtils::bufferBarrier(
                        g.buffer, 0, VK_WHOLE_SIZE,
                        g.lastStageMask, g.lastAccessMask,
                        decl.stageMask, decl.accessMask);

                    bufferBarriers.push_back(barrier);
                }

                g.lastStageMask = decl.stageMask;
                g.lastAccessMask = decl.accessMask;
            }

            if (!imageBarriers.empty() || !bufferBarriers.empty()) {
                VkDependencyInfo dep {};
                dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dep.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
                dep.pImageMemoryBarriers = imageBarriers.data();
                dep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
                dep.pBufferMemoryBarriers = bufferBarriers.data();

                vkCmdPipelineBarrier2(cmdBuffer, &dep);
            }

            if (activeProfiling) {
                pushProfileMarker(cmdBuffer, pass.passNode->getName());
            }

            pass.passNode->execute(cmdBuffer, frameInfo);

            if (activeProfiling) {
                popProfileMarker(cmdBuffer);
            }
        }

        if (activeProfiling) {
            frameMarkers[currentFrame] = activeMarkers;
        }

        if (activeProfiling && framesProfiledCount > Config::MAX_FRAMES_IN_FLIGHT) {
            uint32_t prevFrame = (currentFrame + Config::MAX_FRAMES_IN_FLIGHT - 1) % Config::MAX_FRAMES_IN_FLIGHT;
            uint32_t prevOffset = prevFrame * 256;
            auto &prevMarkers = frameMarkers[prevFrame];
            uint32_t queryCount = static_cast<uint32_t>(prevMarkers.size() * 2);
            
            if (queryCount > 0) {
                std::vector<uint64_t> timestamps(queryCount, 0);
                VkResult res = vkGetQueryPoolResults(
                    device.getDevice(),
                    profilerQueryPool,
                    prevOffset,
                    queryCount,
                    queryCount * sizeof(uint64_t),
                    timestamps.data(),
                    sizeof(uint64_t),
                    VK_QUERY_RESULT_64_BIT
                );

                if (res == VK_SUCCESS) {
                    float periodNs = device.getDeviceProperties().limits.timestampPeriod;
                    for (const auto &marker : prevMarkers) {
                        uint64_t tStart = timestamps[marker.queryIndexStart];
                        uint64_t tEnd = timestamps[marker.queryIndexEnd];
                        if (tEnd >= tStart && tStart > 0) {
                            double passMs = static_cast<double>(tEnd - tStart) * static_cast<double>(periodNs) / 1000000.0;
                            
                            auto it = profileStatsMap.find(marker.name);
                            if (it == profileStatsMap.end()) {
                                PassProfileStats newStats;
                                newStats.name = marker.name;
                                newStats.depth = marker.depth;
                                profileStats.push_back(newStats);
                                profileStatsMap[marker.name] = profileStats.size() - 1;
                                it = profileStatsMap.find(marker.name);
                            }
                            
                            auto &s = profileStats[it->second];
                            s.totalTimeMs += passMs;
                            s.minTimeMs = std::min(s.minTimeMs, passMs);
                            s.maxTimeMs = std::max(s.maxTimeMs, passMs);
                            s.samples++;
                        }
                    }
                }
            }

            if (elapsedSeconds >= 35.0) {
                profileSummaryPrinted = true;
                printProfileSummaryTable();
            }
        }
    }

    void RenderGraph::pushProfileMarker(VkCommandBuffer cmd, const std::string &name) {
        if (profilerQueryPool == VK_NULL_HANDLE || currentQueryCount >= 254) return;
        uint32_t startIdx = currentQueryCount++;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, profilerQueryPool, currentFrameOffset + startIdx);
        activeMarkers.push_back({name, startIdx, 0, currentProfileDepth++});
    }

    void RenderGraph::popProfileMarker(VkCommandBuffer cmd) {
        if (profilerQueryPool == VK_NULL_HANDLE || currentQueryCount >= 255) return;
        uint32_t endIdx = currentQueryCount++;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, profilerQueryPool, currentFrameOffset + endIdx);

        for (auto it = activeMarkers.rbegin(); it != activeMarkers.rend(); ++it) {
            if (it->queryIndexEnd == 0) {
                it->queryIndexEnd = endIdx;
                currentProfileDepth--;
                break;
            }
        }
    }

    void RenderGraph::printProfileSummaryTable()
    {
        double totalGpuMs = 0.0;
        std::vector<PassProfileStats> statsList;
        for (const auto &s : profileStats) {
            if (s.samples > 0) {
                statsList.push_back(s);
                if (s.depth == 0) {
                    totalGpuMs += (s.totalTimeMs / s.samples);
                }
            }
        }

        std::cout << "\n========================================================================================\n"
                  << "ENGINE GPU PASS PROFILER SUMMARY (Measured Over 5s Window After 30s Warmup)\n"
                  << "========================================================================================\n"
                  << std::left << std::setw(30) << "Pass Name"
                  << " | " << std::setw(16) << "Avg GPU Time (ms)"
                  << " | " << std::setw(12) << "Min (ms)"
                  << " | " << std::setw(12) << "Max (ms)"
                  << " | " << std::setw(12) << "% GPU Share" << "\n"
                  << "----------------------------------------------------------------------------------------\n";

        for (const auto &s : statsList) {
            double avgMs = s.totalTimeMs / s.samples;

            double pct = 0.0;
            if (s.depth == 0 && totalGpuMs > 0.0) {
                pct = (avgMs / totalGpuMs) * 100.0;
            }
            
            std::string displayName = s.name;
            if (s.depth > 0) {
                displayName = std::string(s.depth * 2, ' ') + "|- " + s.name;
            }

            std::cout << std::left << std::setw(30) << displayName
                      << " | " << std::fixed << std::setprecision(3) << std::setw(16) << avgMs
                      << " | " << std::setw(12) << s.minTimeMs
                      << " | " << std::setw(12) << s.maxTimeMs
                      << " | ";
                      
            if (s.depth == 0) {
                std::cout << std::setprecision(1) << std::setw(11) << pct << " %\n";
            } else {
                std::cout << std::setw(13) << " " << "\n";
            }
        }
        std::cout << "----------------------------------------------------------------------------------------\n"
                  << std::left << std::setw(30) << "Total Measured Pass Time"
                  << " | " << std::fixed << std::setprecision(3) << std::setw(16) << totalGpuMs << " ms\n"
                  << "========================================================================================\n" << std::flush;

        std::cout << "\a" << std::flush;
#ifdef _WIN32
        Beep(750, 300);
#endif
    }

    void RenderGraph::clear()
    {
        registeredPasses.clear();
        imageRegistry.clear();
        bufferRegistry.clear();
    }

    void RenderGraph::markSceneDirty()
    {
        for (auto pass:registeredPasses) {
            pass.passNode->markSceneDirty();
        }
    }

    void RenderGraph::transitionToPresent(VkCommandBuffer cmdBuffer, const std::string &imageName)
    {
        auto it = imageRegistry.find(imageName);
        if (it == imageRegistry.end())
            return;

        GraphImage &g = it->second;
        if (g.layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            return;

        VkImageSubresourceRange range {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = g.mipLevels;
        range.baseArrayLayer = 0;
        range.layerCount = g.arrayLayers;

        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            g.image, g.layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            g.lastStageMask, g.lastAccessMask,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, range);

        VkDependencyInfo dep {};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmdBuffer, &dep);

        g.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        g.lastStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        g.lastAccessMask = VK_ACCESS_2_NONE;
    }



    VkImageView RenderGraph::getImageView(const std::string &name) const
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end()) {
            return it->second.imageView;
        }
        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered image: " + name);
    }

    RenderGraphBuilder::RenderGraphBuilder(std::vector<ImageUsageDeclaration> &imageUsagesList,
                                           std::vector<TransientImageDeclaration> &transientImageUsagesList,
                                           std::vector<BufferUsageDeclaration> &bufferUsagesList):
        imageUsages(imageUsagesList), bufferUsages(bufferUsagesList), transientImageUsages(transientImageUsagesList)
    {}

    void RenderGraphBuilder::readImage(const std::string &name,
                                       VkImageLayout imageLayout,
                                       VkPipelineStageFlags2 stageMask,
                                       VkAccessFlags2 accessMask)
    {
        imageUsages.push_back({name, imageLayout, stageMask, accessMask, ResourceUsageType::Read});
    }

    void RenderGraphBuilder::writeImage(const std::string &name,
                                        VkImageLayout imageLayout,
                                        VkPipelineStageFlags2 stageMask,
                                        VkAccessFlags2 accessMask)
    {
        imageUsages.push_back({name, imageLayout, stageMask, accessMask, ResourceUsageType::Write});
    }

    void RenderGraphBuilder::readBuffer(const std::string &name,
                                        VkPipelineStageFlags2 stageMask,
                                        VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::Read});
    }

    void RenderGraphBuilder::writeBuffer(const std::string &name,
                                         VkPipelineStageFlags2 stageMask,
                                         VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::Write});
    }

    void RenderGraphBuilder::readWriteBuffer(const std::string &name,
                                             VkPipelineStageFlags2 stageMask,
                                             VkAccessFlags2 accessMask)
    {
        bufferUsages.push_back({name, stageMask, accessMask, ResourceUsageType::ReadWrite});
    }

    void RenderGraphBuilder::createTransientImage(
        const std::string &name, VkFormat format, VkExtent2D extent, uint32_t arrayLayers, VkImageUsageFlags usage, VkClearValue clearValue)
    {
        transientImageUsages.push_back({name, format, extent,arrayLayers, usage, clearValue});
    }

    void RenderGraph::updateImageHandle(const std::string &name, VkImage image, VkImageView view, VkExtent2D extent, uint32_t mipLevels)
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end()) {
            if (it->second.image != image) {
                it->second.layout = VK_IMAGE_LAYOUT_UNDEFINED;

                it->second.lastStageMask =
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                it->second.lastAccessMask = VK_ACCESS_2_NONE;
            }

            it->second.image = image;
            it->second.imageView = view;
            it->second.extent = extent;
            it->second.mipLevels = mipLevels;
        }
    }

    void RenderGraph::updateBufferHandle(const std::string &name, VkBuffer buffer, VkDeviceSize size)
    {
        auto it = bufferRegistry.find(name);
        if (it != bufferRegistry.end()) {
            it->second.buffer = buffer;
            it->second.size = size;
        }
    }

    VkImage RenderGraph::getImage(const std::string &name) const
    {
        auto it = imageRegistry.find(name);
        if (it != imageRegistry.end()) {
            return it->second.image;
        }
        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered image: " + name);
    }

    VkDescriptorBufferInfo RenderGraph::getBufferInfo(const std::string &name, int32_t currentFrame)
    {
        auto it = bufferRegistry.find(name);
        if (it != bufferRegistry.end()) {
            return VkDescriptorBufferInfo {it->second.buffer, 0, VK_WHOLE_SIZE};
        }

        throw std::runtime_error("RenderGraph: Attempted to fetch unregistered buffer: " + name);
    }
} // namespace Engine
