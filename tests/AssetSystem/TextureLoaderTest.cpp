#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Common/SyntheticTextureGenerator.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanTexture.h"
#include "Vulkan/VulkanBindlessHeap.h"
#include "AssetSystem/TextureLoader.h"

namespace Engine::Test {

    class TextureLoaderTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    TEST_F(TextureLoaderTest, LoadAndUploadTexture) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);
        VulkanBindlessHeap bindlessHeap(device, 64);

        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        SyntheticTextureRGBA8 synTex = SyntheticTextureGenerator::GenerateCheckerboard(width, height, 8);
        ASSERT_EQ(synTex.pixels.size(), width * height * 4);

        DecodedImageData decoded{};
        decoded.pixelData = synTex.pixels;
        decoded.extent = { width, height, 1 };
        decoded.format = VK_FORMAT_R8G8B8A8_UNORM;
        decoded.mipLevels = 1;
        decoded.arrayLayers = 1;
        decoded.isCompressed = false;

        std::unique_ptr<VulkanTexture> gpuTexture = TextureLoader::UploadToGPU(
            device, memory, decoded, false, "CheckerboardUploadTest");

        ASSERT_NE(gpuTexture, nullptr);
        EXPECT_NE(gpuTexture->GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(gpuTexture->GetImageView(), VK_NULL_HANDLE);
        EXPECT_EQ(gpuTexture->GetFormat(), VK_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(gpuTexture->GetExtent2D().width, width);
        EXPECT_EQ(gpuTexture->GetExtent2D().height, height);
        EXPECT_EQ(gpuTexture->GetMipLevels(), 1u);

        BindlessTextureHandle handle = bindlessHeap.RegisterTexture(
            gpuTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        EXPECT_TRUE(handle.IsValid());
        EXPECT_LT(handle.slot, 64u);

        bindlessHeap.FlushPendingUpdates();
        bindlessHeap.UnregisterTexture(handle);
    }

    TEST_F(TextureLoaderTest, MipmapGeneration) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        constexpr uint32_t width = 64;
        constexpr uint32_t height = 64;
        SyntheticTextureRGBA8 synTex = SyntheticTextureGenerator::GenerateSolidColor(
            width, height, { 128, 64, 32, 255 });

        DecodedImageData decoded{};
        decoded.pixelData = synTex.pixels;
        decoded.extent = { width, height, 1 };
        decoded.format = VK_FORMAT_R8G8B8A8_UNORM;
        decoded.mipLevels = 1;
        decoded.arrayLayers = 1;
        decoded.isCompressed = false;

        // Upload with generateMipmaps = true
        std::unique_ptr<VulkanTexture> gpuTexture = TextureLoader::UploadToGPU(
            device, memory, decoded, true, "MipmapGenTest");

        ASSERT_NE(gpuTexture, nullptr);
        constexpr uint32_t expectedMips = 7;
        EXPECT_EQ(gpuTexture->GetMipLevels(), expectedMips);

        for (uint32_t i = 0; i < expectedMips; ++i) {
            VkImageView mipView = gpuTexture->GetMipView(i);
            EXPECT_NE(mipView, VK_NULL_HANDLE);
        }
    }

    TEST_F(TextureLoaderTest, PreGeneratedMipmapsPreserved) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        constexpr uint32_t width = 32;
        constexpr uint32_t height = 32;
        constexpr uint32_t authoredMips = 3;

        size_t totalBytes = 0;
        std::vector<VkBufferImageCopy> regions;
        for (uint32_t i = 0; i < authoredMips; ++i) {
            uint32_t w = std::max(1u, width >> i);
            uint32_t h = std::max(1u, height >> i);
            VkBufferImageCopy region{};
            region.bufferOffset = totalBytes;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = i;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = { w, h, 1 };
            regions.push_back(region);
            totalBytes += w * h * 4;
        }

        DecodedImageData decoded{};
        decoded.pixelData.resize(totalBytes, 0xFF);
        decoded.extent = { width, height, 1 };
        decoded.format = VK_FORMAT_R8G8B8A8_UNORM;
        decoded.mipLevels = authoredMips;
        decoded.arrayLayers = 1;
        decoded.isCompressed = false;
        decoded.copyRegions = regions;

        std::unique_ptr<VulkanTexture> gpuTexture = TextureLoader::UploadToGPU(
            device, memory, decoded, true, "PreGeneratedMipsTest");

        ASSERT_NE(gpuTexture, nullptr);
        EXPECT_EQ(gpuTexture->GetMipLevels(), authoredMips);
        for (uint32_t i = 0; i < authoredMips; ++i) {
            EXPECT_NE(gpuTexture->GetMipView(i), VK_NULL_HANDLE);
        }
    }

}
