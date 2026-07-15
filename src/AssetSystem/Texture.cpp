#include "AssetSystem/Texture.h"
#include "Vulkan/Device.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Vulkan/VkUtils.h"

namespace Engine {
    void Texture::updateDescriptor()
    {
        descriptor.sampler = sampler;
        descriptor.imageView = view;
        descriptor.imageLayout = imageLayout;
    }


    void Texture::destroy()
    {
        if (!device)
            return;

        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device->getDevice(), view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(device->getAllocator(), image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device->getDevice(), sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }
    }

    ktxResult Texture::loadKTXFile(std::string filename, ktxTexture **target)
    {
        ktxResult result = KTX_SUCCESS;

        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("Texture: File does not exist: " + filename);
        }

        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Texture: Failed to load texture: " + filename);
        }

        return result;
    }

    void Texture::transitionImageLayout(VkCommandBuffer commandBuffer,
                                        VkImage image,
                                        VkImageLayout oldLayout,
                                        VkImageLayout newLayout,
                                        VkImageSubresourceRange subresourceRange)
    {
        VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            srcAccessMask = VK_ACCESS_2_NONE;
            dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                   newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            dstAccessMask = VK_ACCESS_2_NONE;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            srcAccessMask = VK_ACCESS_2_NONE;
            dstStageMask =
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            image, oldLayout, newLayout, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask, subresourceRange);

        VkDependencyInfo dependencyInfo {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }

    void Texture2D::loadFromFile(std::string filename,
                                 VkFormat format,
                                 Device *device,
                                 ResourceHeap &resourceHeap,
                                 VkFilter filter,
                                 VkImageUsageFlags imageUsageFlags,
                                 VkImageLayout imageLayout)
    {
        ktxTexture *ktxTexture;
        ktxResult result = loadKTXFile(filename, &ktxTexture);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Failed to load KTX texture: " + filename);
        }

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

        Buffer stgBuffer {
            *device, ktxTextureSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(ktxTextureData, ktxTextureSize, 0);
        stgBuffer.flush(ktxTextureSize, 0);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < mipLevels; i++) {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
            assert(result == KTX_SUCCESS);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        VkImageCreateInfo imageCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                           .imageType = VK_IMAGE_TYPE_2D,
                                           .format = format,
                                           .extent = {width, height, 1},
                                           .mipLevels = mipLevels,
                                           .arrayLayers = 1,
                                           .samples = VK_SAMPLE_COUNT_1_BIT,
                                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                                           .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult imageResult =
            vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {});
        if (imageResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture image for: " + filename);
        }

        VkCommandBuffer copyCmd = device->beginSingleTimeCommands();

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        transitionImageLayout(
            copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        vkCmdCopyBufferToImage(copyCmd,
                               stgBuffer.getBuffer(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(bufferCopyRegions.size()),
                               bufferCopyRegions.data());

        this->imageLayout = imageLayout;
        transitionImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

        device->endSingleTimeCommands(copyCmd);

        ktxTexture_Destroy(ktxTexture);

        VkSamplerCreateInfo samplerCreateInfo {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                               .magFilter = filter,
                                               .minFilter = filter,
                                               .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .mipLodBias = 0.0f,
                                               .anisotropyEnable = VK_TRUE,
                                               .maxAnisotropy = device->getMaxAnisotropy(),
                                               .compareOp = VK_COMPARE_OP_NEVER,
                                               .minLod = 0.0f,
                                               .maxLod = (float)mipLevels,
                                               .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
        if (vkCreateSampler(device->getDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to create sampler for texture: " + filename);
        }

        VkImageViewCreateInfo viewCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                              .image = image,
                                              .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                              .format = format,
                                              .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1}};
        if (vkCreateImageView(device->getDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Texture: failed to create texture image view!");
        }

        updateDescriptor();

        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void Texture2D::fromBuffer(void *buffer,
                               VkDeviceSize bufferSize,
                               VkFormat format,
                               uint32_t texWidth,
                               uint32_t texHeight,
                               Device *device,
                               ResourceHeap &resourceHeap,
                               VkFilter filter,
                               VkImageUsageFlags imageUsageFlags,
                               VkImageLayout imageLayout)
    {
        assert(buffer);

        this->device = device;

        stbi_uc *pixels = nullptr;
        VkDeviceSize imageSize = 0;

        if (texWidth == 0 || texHeight == 0) {
            int imgWidth, imgHeight, imgChannels;
            pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(buffer),
                                           static_cast<int>(bufferSize),
                                           &imgWidth,
                                           &imgHeight,
                                           &imgChannels,
                                           STBI_rgb_alpha);

            if (!pixels) {
                throw std::runtime_error("Texture: Failed to decode image from memory!");
            }

            this->width = static_cast<uint32_t>(imgWidth);
            this->height = static_cast<uint32_t>(imgHeight);
            imageSize = this->width * this->height * 4;
        } else {
            this->width = texWidth;
            this->height = texHeight;
            imageSize = bufferSize;
            pixels = reinterpret_cast<stbi_uc *>(buffer);
        }

        mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

        Buffer stgBuffer {*device, imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(pixels, imageSize, 0);
        stgBuffer.flush(imageSize, 0);

        if (texWidth == 0 || texHeight == 0) {
            stbi_image_free(pixels);
        }

        VkImageCreateInfo imageCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {width, height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult imageResult =
            vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {});
        if (imageResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture image");
        }

        VkCommandBuffer commandBuffer = device->beginSingleTimeCommands();

        VkImageSubresourceRange subresourceRange {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        transitionImageLayout(
            commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        VkBufferImageCopy bufferCopyRegion {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = width;
        bufferCopyRegion.imageExtent.height = height;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = 0;

        vkCmdCopyBufferToImage(
            commandBuffer, stgBuffer.getBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

        int32_t mipWidth = width;
        int32_t mipHeight = height;

        for (uint32_t i = 1; i < mipLevels; i++) {
            VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1});

            VkDependencyInfo depInfo {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(commandBuffer, &depInfo);

            VkImageBlit blit {};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           filter);

            barrier = VkUtils::imageBarrier(
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageLayout,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1});

            vkCmdPipelineBarrier2(commandBuffer, &depInfo);

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1});

        VkDependencyInfo depInfo {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &depInfo);

        this->imageLayout = imageLayout;

        device->endSingleTimeCommands(commandBuffer);

        VkImageViewCreateInfo viewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};

        if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Texture: failed to create texture image view!");
        }

        VkSamplerCreateInfo samplerInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = filter;
        samplerInfo.minFilter = filter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = device->getMaxAnisotropy();

        if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Texture: failed to create texture sampler!");
        }

        updateDescriptor();

        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void Texture2D::fromKTXPtr(void *ktxTexPtr,
                               Device *device,
                               ResourceHeap &resourceHeap,
                               bool isSRGB,
                               VkFilter filter,
                               VkImageUsageFlags imageUsageFlags,
                               VkImageLayout imageLayout)
    {
        if (!ktxTexPtr)
            throw std::runtime_error("Texture: KTX Pointer is null");
        
        ktxTexture *ktxTexture = reinterpret_cast<::ktxTexture *>(ktxTexPtr);

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;

        VkFormat format = ktxTexture_GetVkFormat(ktxTexture);

        if (isSRGB) {
            if (format == VK_FORMAT_BC7_UNORM_BLOCK)
                format = VK_FORMAT_BC7_SRGB_BLOCK;
            else if (format == VK_FORMAT_R8G8B8A8_UNORM)
                format = VK_FORMAT_R8G8B8A8_SRGB;
            else if (format == VK_FORMAT_BC3_UNORM_BLOCK)
                format = VK_FORMAT_BC3_SRGB_BLOCK;
            else if (format == VK_FORMAT_ASTC_4x4_UNORM_BLOCK)
                format = VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        }

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

        Buffer stgBuffer {
            *device, ktxTextureSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(ktxTextureData, ktxTextureSize, 0);
        stgBuffer.flush(ktxTextureSize, 0);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < mipLevels; i++) {
            ktx_size_t offset;
            ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        VkImageCreateInfo imageCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                           .imageType = VK_IMAGE_TYPE_2D,
                                           .format = format,
                                           .extent = {width, height, 1},
                                           .mipLevels = mipLevels,
                                           .arrayLayers = 1,
                                           .samples = VK_SAMPLE_COUNT_1_BIT,
                                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                                           .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {}) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate KTX texture image");
        }

        VkCommandBuffer copyCmd = device->beginSingleTimeCommands();
        VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};

        transitionImageLayout(
            copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
        vkCmdCopyBufferToImage(copyCmd,
                               stgBuffer.getBuffer(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(bufferCopyRegions.size()),
                               bufferCopyRegions.data());

        this->imageLayout = imageLayout;
        transitionImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

        device->endSingleTimeCommands(copyCmd);

        // Clean up the CPU texture memory now that it's on the GPU
        ktxTexture_Destroy(ktxTexture);

        // Create View and Sampler
        VkImageViewCreateInfo viewCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                              .image = image,
                                              .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                              .format = format,
                                              .subresourceRange = subresourceRange};
        vkCreateImageView(device->getDevice(), &viewCreateInfo, nullptr, &view);

        VkSamplerCreateInfo samplerCreateInfo {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                               .magFilter = filter,
                                               .minFilter = filter,
                                               .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               .mipLodBias = 0.0f,
                                               .anisotropyEnable = VK_TRUE,
                                               .maxAnisotropy = device->getMaxAnisotropy(),
                                               .compareOp = VK_COMPARE_OP_NEVER,
                                               .minLod = 0.0f,
                                               .maxLod = (float)mipLevels,
                                               .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
        vkCreateSampler(device->getDevice(), &samplerCreateInfo, nullptr, &sampler);

        updateDescriptor();
        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void Texture2D::createDefaultTexture(
        Device *device, uint8_t r, uint8_t g, uint8_t b, uint8_t a, ResourceHeap &resourceHeap)
    {
        VkDeviceSize imageSize = 4;
        unsigned char pixels[4] = {r, g, b, a};
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        Buffer stgBuffer {*device, imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(pixels, imageSize, 0);
        stgBuffer.flush(imageSize, 0);

        VkImageCreateInfo imageCreateInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = 1;
        imageCreateInfo.extent.height = 1;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult imageResult =
            vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {});
        if (imageResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture image");
        }

        VkCommandBuffer commandBuffer = device->beginSingleTimeCommands();


        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        VkDependencyInfo depInfo {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(commandBuffer, &depInfo);

        VkBufferImageCopy region {};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {1, 1, 1};

        vkCmdCopyBufferToImage(
            commandBuffer, stgBuffer.getBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier2 barrier2 = VkUtils::imageBarrier(
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        VkDependencyInfo depInfo2 {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo2.imageMemoryBarrierCount = 1;
        depInfo2.pImageMemoryBarriers = &barrier2;
        vkCmdPipelineBarrier2(commandBuffer, &depInfo2);

        device->endSingleTimeCommands(commandBuffer);

        VkImageViewCreateInfo viewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to create default texture image view");
        }

        VkSamplerCreateInfo samplerInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        if (vkCreateSampler(device->getDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to create default texture sampler");
        }

        this->device = device;
        width = 1;
        height = 1;
        mipLevels = 1;
        layerCount = 1;
        imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        updateDescriptor();

        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void Texture2DArray::loadFromFile(std::string filename,
                                      VkFormat format,
                                      Device *device,
                                      ResourceHeap &resourceHeap,
                                      VkImageUsageFlags imageUsageFlags,
                                      VkImageLayout imageLayout)
    {
        ktxTexture *ktxTexture;
        ktxResult result = loadKTXFile(filename, &ktxTexture);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Texture: Failed to load KTX texture array: " + filename);
        }

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        layerCount = ktxTexture->numLayers;
        mipLevels = ktxTexture->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

        Buffer stgBuffer {
            *device, ktxTextureSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(ktxTextureData, ktxTextureSize, 0);
        stgBuffer.flush(ktxTextureSize, 0);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t layer = 0; layer < layerCount; layer++) {
            for (uint32_t level = 0; level < mipLevels; level++) {
                ktx_size_t offset;
                KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, layer, 0, &offset);
                assert(result == KTX_SUCCESS);

                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> level);
                bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> level);
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }


        VkImageCreateInfo imageCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                           .imageType = VK_IMAGE_TYPE_2D,
                                           .format = format,
                                           .extent = {width, height, 1},
                                           .mipLevels = mipLevels,
                                           .arrayLayers = layerCount,
                                           .samples = VK_SAMPLE_COUNT_1_BIT,
                                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                                           .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo;
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult imageResult =
            vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {});
        if (imageResult != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture array image for: " + filename);
        }

        VkCommandBuffer copyCmd = device->beginSingleTimeCommands();

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = layerCount;

        transitionImageLayout(
            copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        vkCmdCopyBufferToImage(copyCmd,
                               stgBuffer.getBuffer(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(bufferCopyRegions.size()),
                               bufferCopyRegions.data());

        this->imageLayout = imageLayout;

        transitionImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

        device->endSingleTimeCommands(copyCmd);

        ktxTexture_Destroy(ktxTexture);

        VkSamplerCreateInfo samplerCreateInfo {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                               .magFilter = VK_FILTER_LINEAR,
                                               .minFilter = VK_FILTER_LINEAR,
                                               .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .mipLodBias = 0.0f,
                                               .anisotropyEnable = VK_TRUE,
                                               .maxAnisotropy = device->getMaxAnisotropy(),
                                               .compareOp = VK_COMPARE_OP_NEVER,
                                               .minLod = 0.0f,
                                               .maxLod = (float)mipLevels,
                                               .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
        if (vkCreateSampler(device->getDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to create sampler for texture: " + filename);
        }

        VkImageViewCreateInfo viewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, layerCount}};
        if (vkCreateImageView(device->getDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Texture: failed to create texture image view!");
        }

        updateDescriptor();

        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void TextureCubeMap::loadFromFile(std::string filename,
                                      VkFormat format,
                                      Device *device,
                                      ResourceHeap &resourceHeap,
                                      VkImageUsageFlags imageUsageFlags,
                                      VkImageLayout imageLayout)
    {
        ktxTexture *ktxTexture;
        ktxResult result = loadKTXFile(filename, &ktxTexture);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Failed to load cubemap texture from " + filename);
        }

        this->device = device;
        width = ktxTexture->baseWidth;
        height = ktxTexture->baseHeight;
        mipLevels = ktxTexture->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

        Buffer stgBuffer {
            *device, ktxTextureSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};
        stgBuffer.writeToBuffer(ktxTextureData, ktxTextureSize, 0);
        stgBuffer.flush(ktxTextureSize, 0);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t face = 0; face < 6; face++) {
            for (uint32_t level = 0; level < mipLevels; level++) {
                ktx_size_t offset;
                KTX_error_code ktxResult = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
                assert(ktxResult == KTX_SUCCESS);

                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> level);
                bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> level);
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        VkImageCreateInfo imageCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                           .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                           .imageType = VK_IMAGE_TYPE_2D,
                                           .format = format,
                                           .extent = {width, height, 1},
                                           .mipLevels = mipLevels,
                                           .arrayLayers = 6,
                                           .samples = VK_SAMPLE_COUNT_1_BIT,
                                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                                           .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult imageResult =
            vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {});
        if (imageResult != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to allocate cubemap image for: " + filename);
        }

        VkCommandBuffer copyCmd = device->beginSingleTimeCommands();

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 6;

        transitionImageLayout(
            copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        vkCmdCopyBufferToImage(copyCmd,
                               stgBuffer.getBuffer(),
                               image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(bufferCopyRegions.size()),
                               bufferCopyRegions.data());

        this->imageLayout = imageLayout;

        transitionImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);

        device->endSingleTimeCommands(copyCmd);

        ktxTexture_Destroy(ktxTexture);

        VkSamplerCreateInfo samplerCreateInfo {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                               .magFilter = VK_FILTER_LINEAR,
                                               .minFilter = VK_FILTER_LINEAR,
                                               .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .mipLodBias = 0.0f,
                                               .anisotropyEnable = VK_TRUE,
                                               .maxAnisotropy = device->getMaxAnisotropy(),
                                               .compareOp = VK_COMPARE_OP_NEVER,
                                               .minLod = 0.0f,
                                               .maxLod = (float)mipLevels,
                                               .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
        if (vkCreateSampler(device->getDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Texture: Failed to create sampler for texture: " + filename);
        }

        VkImageViewCreateInfo viewCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                              .image = image,
                                              .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
                                              .format = format,
                                              .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6}};
        if (vkCreateImageView(device->getDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Texture: failed to create texture image view!");
        }

        updateDescriptor();

        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }

    void TextureCubeMap::loadFromFileSTB(std::array<std::string, 6> filenames,
                                         VkFormat format,
                                         Device *device,
                                         ResourceHeap &resourceHeap,
                                         VkImageUsageFlags imageUsageFlags,
                                         VkImageLayout imageLayout)
    {
        this->device = device;
        this->layerCount = 6;

        bool isHDR = stbi_is_hdr(filenames[0].c_str());
        int texWidth = 0, texHeight = 0, texChannels = 0;

        if (!stbi_info(filenames[0].c_str(), &texWidth, &texHeight, &texChannels)) {
            throw std::runtime_error("TextureCubeMap: Failed to read image info from: " + filenames[0]);
        }
        this->width = static_cast<uint32_t>(texWidth);
        this->height = static_cast<uint32_t>(texHeight);
        this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;


        VkDeviceSize faceSize = width * height * 4 * (isHDR ? sizeof(float) : sizeof(stbi_uc));
        VkDeviceSize totalImageSize = faceSize * 6;

        Buffer stgBuffer {
            *device, totalImageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0};

        for (uint32_t i = 0; i < 6; i++) {
            int w, h, c;
            void *pixels = nullptr;

            if (isHDR) {
                pixels = stbi_loadf(filenames[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
            } else {
                pixels = stbi_load(filenames[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
            }

            if (!pixels) {
                throw std::runtime_error("TextureCubeMap: Failed to load cubemap face: " + filenames[i]);
            }
            if (w != width || h != height) {
                stbi_image_free(pixels);
                throw std::runtime_error("TextureCubeMap: Cubemap faces have differing dimensions! Check face: " +
                                         filenames[i]);
            }

            stgBuffer.writeToBuffer(pixels, faceSize, faceSize * i);
            stbi_image_free(pixels);
        }
        stgBuffer.flush(totalImageSize, 0);


        VkImageCreateInfo imageCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {width, height, 1},
            .mipLevels = mipLevels,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(device->getAllocator(), &imageCreateInfo, &allocInfo, &image, &allocation, {}) !=
            VK_SUCCESS) {
            throw std::runtime_error("TextureCubeMap: Failed to allocate cubemap image");
        }

        VkCommandBuffer copyCmd = device->beginSingleTimeCommands();

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 6;

        transitionImageLayout(
            copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 6;
        bufferCopyRegion.imageExtent = {width, height, 1};
        bufferCopyRegion.bufferOffset = 0;

        vkCmdCopyBufferToImage(
            copyCmd, stgBuffer.getBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

        int32_t mipWidth = width;
        int32_t mipHeight = height;

        for (uint32_t i = 1; i < mipLevels; i++) {
            VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 6});

            VkDependencyInfo depInfo {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(copyCmd, &depInfo);

            VkImageBlit blit {};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 6;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 6;

            vkCmdBlitImage(copyCmd,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

            barrier = VkUtils::imageBarrier(
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageLayout,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 6});

            vkCmdPipelineBarrier2(copyCmd, &depInfo);

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        VkImageMemoryBarrier2 barrier = VkUtils::imageBarrier(
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 6});

        VkDependencyInfo depInfoFinal {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfoFinal.imageMemoryBarrierCount = 1;
        depInfoFinal.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(copyCmd, &depInfoFinal);

        this->imageLayout = imageLayout;
        device->endSingleTimeCommands(copyCmd);

        VkSamplerCreateInfo samplerCreateInfo {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                               .magFilter = VK_FILTER_LINEAR,
                                               .minFilter = VK_FILTER_LINEAR,
                                               .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                               .mipLodBias = 0.0f,
                                               .anisotropyEnable = VK_TRUE,
                                               .maxAnisotropy = device->getMaxAnisotropy(),
                                               .compareOp = VK_COMPARE_OP_NEVER,
                                               .minLod = 0.0f,
                                               .maxLod = static_cast<float>(mipLevels),
                                               .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};

        if (vkCreateSampler(device->getDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("TextureCubeMap: Failed to create sampler");
        }

        VkImageViewCreateInfo viewCreateInfo {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                              .image = image,
                                              .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
                                              .format = format,
                                              .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6}};

        if (vkCreateImageView(device->getDevice(), &viewCreateInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("TextureCubeMap: Failed to create image view");
        }

        updateDescriptor();
        this->heapHandle = resourceHeap.registerTexture(this->descriptor);
    }
} // namespace Engine
