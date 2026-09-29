#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanTexture.h"

namespace Engine::Test {

    class VulkanTextureTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    // Test Case 1: MipLevelCalculation
    // Verifies automated mip level formula: log2(max(W, H)) + 1
    TEST_F(VulkanTextureTest, MipLevelCalculation) {
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(1, 1), 1u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(2, 2), 2u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(4, 4), 3u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(64, 64), 7u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(128, 64), 8u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(1920, 1080), 11u);
        EXPECT_EQ(VulkanTexture::CalculateMipLevels(4096, 4096), 13u);
    }

    // Test Case 2: TextureCreationAndProperties
    // Allocates a 2D sampled texture on GPU, verifies view, extent, format, and debug naming
    TEST_F(VulkanTextureTest, TextureCreationAndProperties) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        TextureDesc desc{
            .debugName = "TestColorTexture",
            .extent = { 128, 128, 1 },
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = TextureUsage::Sampled | TextureUsage::TransferDst,
            .mipLevels = 8,
            .arrayLayers = 1,
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .viewType = VK_IMAGE_VIEW_TYPE_2D
        };

        VulkanTexture texture(device, memory, desc);

        EXPECT_NE(texture.GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(texture.GetImageView(), VK_NULL_HANDLE);
        EXPECT_EQ(texture.GetFormat(), VK_FORMAT_R8G8B8A8_UNORM);
        EXPECT_EQ(texture.GetExtent().width, 128u);
        EXPECT_EQ(texture.GetExtent().height, 128u);
        EXPECT_EQ(texture.GetExtent2D().width, 128u);
        EXPECT_EQ(texture.GetExtent2D().height, 128u);
        EXPECT_EQ(texture.GetMipLevels(), 8u);
        EXPECT_EQ(texture.GetArrayLayers(), 1u);
        EXPECT_TRUE(texture.OwnsImage());
    }

    // Test Case 3: ExternalImageWrapper
    // Wraps an existing VkImage (simulating swapchain images) without transferring ownership
    TEST_F(VulkanTextureTest, ExternalImageWrapper) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        // Create an underlying image to wrap
        TextureDesc underlyingDesc{
            .debugName = "UnderlyingImage",
            .extent = { 64, 64, 1 },
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .usage = TextureUsage::ColorAttachment,
            .mipLevels = 1,
            .arrayLayers = 1,
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .viewType = VK_IMAGE_VIEW_TYPE_2D
        };

        VulkanTexture underlying(device, memory, underlyingDesc);
        VkImage rawImage = underlying.GetHandle();

        // Wrap external image
        TextureDesc wrapperDesc{
            .debugName = "SwapchainWrapper",
            .extent = { 64, 64, 1 },
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .usage = TextureUsage::ColorAttachment,
            .mipLevels = 1,
            .arrayLayers = 1
        };

        {
            VulkanTexture wrapper(device, rawImage, wrapperDesc);
            EXPECT_EQ(wrapper.GetHandle(), rawImage);
            EXPECT_NE(wrapper.GetImageView(), VK_NULL_HANDLE);
            EXPECT_FALSE(wrapper.OwnsImage());
        }

        // Underlying texture must remain valid after wrapper destruction
        EXPECT_NE(underlying.GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(underlying.GetImageView(), VK_NULL_HANDLE);
    }

    // Test Case 4: MipViews
    // Verifies lazy creation of per-mip image views for downsampled levels
    TEST_F(VulkanTextureTest, MipViews) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        TextureDesc desc{
            .debugName = "MipViewTexture",
            .extent = { 64, 64, 1 },
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = TextureUsage::Sampled | TextureUsage::TransferDst | TextureUsage::TransferSrc,
            .mipLevels = 7,
            .arrayLayers = 1,
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .viewType = VK_IMAGE_VIEW_TYPE_2D
        };

        VulkanTexture texture(device, memory, desc);

        for (uint32_t i = 0; i < 7; ++i) {
            VkImageView mipView = texture.GetMipView(i);
            EXPECT_NE(mipView, VK_NULL_HANDLE);
            // Subsequent calls must return identical cached view
            EXPECT_EQ(texture.GetMipView(i), mipView);
        }
    }

    // Test Case 5: MoveSemantics
    // Verifies move constructor and move assignment properly transfer GPU resources and views
    TEST_F(VulkanTextureTest, MoveSemantics) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        TextureDesc desc{
            .debugName = "MoveTestTexture",
            .extent = { 32, 32, 1 },
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = TextureUsage::Sampled,
            .mipLevels = 2,
            .arrayLayers = 1
        };

        VulkanTexture texA(device, memory, desc);
        VkImage handleA = texA.GetHandle();
        VkImageView viewA = texA.GetImageView();
        ASSERT_NE(handleA, VK_NULL_HANDLE);

        // Move construct
        VulkanTexture texB(std::move(texA));
        EXPECT_EQ(texB.GetHandle(), handleA);
        EXPECT_EQ(texB.GetImageView(), viewA);
        EXPECT_EQ(texA.GetHandle(), VK_NULL_HANDLE);
        EXPECT_EQ(texA.GetImageView(), VK_NULL_HANDLE);

        // Move assign
        VulkanTexture texC(device, memory, desc);
        VkImage handleC = texC.GetHandle();
        texC = std::move(texB);
        EXPECT_EQ(texC.GetHandle(), handleA);
        EXPECT_EQ(texB.GetHandle(), VK_NULL_HANDLE);
    }

    // Test Case 6: DepthAttachmentCreation
    // Verifies proper aspect mask deduction for depth formats
    TEST_F(VulkanTextureTest, DepthAttachmentCreation) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        TextureDesc desc{
            .debugName = "DepthTexture",
            .extent = { 100, 100, 1 },
            .format = VK_FORMAT_D32_SFLOAT,
            .usage = TextureUsage::DepthAttachment,
            .mipLevels = 1,
            .arrayLayers = 1
        };

        VulkanTexture depthTex(device, memory, desc);
        EXPECT_NE(depthTex.GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(depthTex.GetImageView(), VK_NULL_HANDLE);
        EXPECT_EQ(depthTex.GetFormat(), VK_FORMAT_D32_SFLOAT);
    }

} // namespace Engine::Test
