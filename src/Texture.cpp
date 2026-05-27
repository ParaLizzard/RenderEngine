//
// Created by Martin Varga on 26.05.2026.
//

#include "Texture.h"

#include <cassert>
#include <iostream>

#include "Buffer.h"

namespace Engine
{
    void Texture::updateDescriptor()
    {
        descriptor.sampler = sampler;
        descriptor.imageView = view;
        descriptor.imageLayout = imageLayout;
    }

    void Texture::destroy()
    {
        vkDestroyImageView(device->getDevice(), view, nullptr);
        vmaDestroyImage(device->getAllocator(), image, allocation);

        if (sampler)
        {
            vkDestroySampler(device->getDevice(), sampler, nullptr);
        }
    }

    ktxResult Texture::loadKTXFile(std::string filename, ktxTexture **target)
    {
        ktxResult result = KTX_SUCCESS;

        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("File does not exist: " + filename);
        }

        result = ktxTexture_CreateFromNamedFile(filename.c_str(),
                                                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                target);

        if (result != KTX_SUCCESS) {
            throw std::runtime_error("Failed to load texture: " + filename);
        }

        return result;
    }

    void Texture::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
    {
        if (commandBuffer == VK_NULL_HANDLE)
        {
            return;
        }

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = 0;
        VkFence fence;
        vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &fence);
        // Submit to the queue
        vkQueueSubmit(queue, 1, &submitInfo, fence);
        // Wait for the fence to signal that command buffer has finished executing
        vkWaitForFences(device->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(device->getDevice(), fence, nullptr);
        if (free)
        {
            vkFreeCommandBuffers(device->getDevice(), pool, 1, &commandBuffer);
        }
    }

    void Texture::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
    {
        return flushCommandBuffer(commandBuffer, queue, device->getCommandPool(), free);
    }


    void Texture::transitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageSubresourceRange subresourceRange)
    {
        // Initialize the unified modern barrier struct
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.image = image;
        barrier.subresourceRange = subresourceRange;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // Automatically resolve pipeline stages and access masks based on layouts
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout ==
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_NONE;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        {
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    }

    VkCommandBuffer Texture::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin)
    {
        VkCommandBuffer cmdBuffer;

        VkCommandBufferAllocateInfo cmdBufAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = level,
            .commandBufferCount = 1
        };
        vkAllocateCommandBuffers(device->getDevice(), &cmdBufAllocateInfo, &cmdBuffer);

        // If requested, also start recording for the new command buffer
        if (begin)
        {
            VkCommandBufferBeginInfo cmdBufInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
            };
            vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo);
        }

        return cmdBuffer;
    }

    VkCommandBuffer Texture::createCommandBuffer(VkCommandBufferLevel level, bool begin)
    {
        return createCommandBuffer(level, device->getCommandPool(), begin);
    }

    void Texture2D::loadFromFile(std::string filename, VkFormat format, Device *device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
	{
		ktxTexture* ktxTexture;
		ktxResult result = loadKTXFile(filename, &ktxTexture);

		if (result != KTX_SUCCESS) {
			std::cerr << "Failed to load texture: " << filename << std::endl;
			return;
		}

		this->device = device;
		width = ktxTexture->baseWidth;
		height = ktxTexture->baseHeight;
		mipLevels = ktxTexture->numLevels;

		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

		VkBuffer stagingBuffer;
    	VmaAllocation stagingAllocation;

    	Buffer stgBuffer{*device, ktxTextureSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, {}, 0 };
    	stgBuffer.writeToBuffer(ktxTextureData, ktxTextureSize, 0);

		std::vector<VkBufferImageCopy> bufferCopyRegions;
		for (uint32_t i = 0; i < mipLevels; i++)
		{
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

		// Create optimal tiled target image
		// IMPORTANT: Fields must be in declaration order for C++20 designated initializers
		VkImageCreateInfo imageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent = { width, height, 1 },
			.mipLevels = mipLevels,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

    	VmaAllocationCreateInfo* allocInfo{};
    	allocInfo.

		vmaCreateImage(device->getAllocator(), imageCreateInfo,&image, allocation, {});

		vkCreateImage(device->device(), &imageCreateInfo, nullptr, &image);

		vkGetImageMemoryRequirements(device->device(), image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = device->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vkAllocateMemory(device->device(), &memAllocInfo, nullptr, &deviceMemory);
		vkBindImageMemory(device->device(), image, deviceMemory, 0);

		VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;
		setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageLayout,
			subresourceRange);

		flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		vkFreeMemory(device->device(), stagingMemory, nullptr);
		vkDestroyBuffer(device->device(), stagingBuffer, nullptr);

		ktxTexture_Destroy(ktxTexture);

		// Create sampler
		// IMPORTANT: Fields must be in declaration order for C++20 designated initializers
		VkSamplerCreateInfo samplerCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
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
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
		};
		vkCreateSampler(device->device(), &samplerCreateInfo, nullptr, &sampler);

		// Create image view
		// IMPORTANT: Fields must be in declaration order for C++20 designated initializers
		VkImageViewCreateInfo viewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
		};
		vkCreateImageView(device->device(), &viewCreateInfo, nullptr, &view);

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}
}
