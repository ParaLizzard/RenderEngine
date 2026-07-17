#include <gtest/gtest.h>
#include "Vulkan/VkUtils.h"

// =============================================================================
// isDepthFormat tests
// =============================================================================

class VkUtilsIsDepthFormatTest : public ::testing::TestWithParam<std::pair<VkFormat, bool>> {};

TEST_P(VkUtilsIsDepthFormatTest, IdentifiesDepthFormatsCorrectly)
{
    auto [format, expectedIsDepth] = GetParam();
    EXPECT_EQ(Engine::VkUtils::isDepthFormat(format), expectedIsDepth)
        << "Format enum value: " << static_cast<int>(format);
}

INSTANTIATE_TEST_SUITE_P(
    DepthFormats,
    VkUtilsIsDepthFormatTest,
    ::testing::Values(
        // All depth/stencil formats → true
        std::make_pair(VK_FORMAT_D16_UNORM,          true),
        std::make_pair(VK_FORMAT_X8_D24_UNORM_PACK32, true),
        std::make_pair(VK_FORMAT_D32_SFLOAT,         true),
        std::make_pair(VK_FORMAT_S8_UINT,            true),
        std::make_pair(VK_FORMAT_D16_UNORM_S8_UINT,  true),
        std::make_pair(VK_FORMAT_D24_UNORM_S8_UINT,  true),
        std::make_pair(VK_FORMAT_D32_SFLOAT_S8_UINT, true),

        // Color / non-depth formats → false
        std::make_pair(VK_FORMAT_R8G8B8A8_UNORM,       false),
        std::make_pair(VK_FORMAT_R8G8B8A8_SRGB,        false),
        std::make_pair(VK_FORMAT_B8G8R8A8_UNORM,       false),
        std::make_pair(VK_FORMAT_B8G8R8A8_SRGB,        false),
        std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT,   false),
        std::make_pair(VK_FORMAT_R32G32B32A32_SFLOAT,   false),
        std::make_pair(VK_FORMAT_R8_UNORM,              false),
        std::make_pair(VK_FORMAT_R32_SFLOAT,            false),
        std::make_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32, false),
        std::make_pair(VK_FORMAT_UNDEFINED,             false)
    )
);

// =============================================================================
// isDepthFormat compile-time (constexpr) verification
// =============================================================================

TEST(VkUtilsConstexpr, IsDepthFormatIsConstexpr)
{
    // Verify the function is usable in constexpr contexts
    static_assert(Engine::VkUtils::isDepthFormat(VK_FORMAT_D32_SFLOAT) == true);
    static_assert(Engine::VkUtils::isDepthFormat(VK_FORMAT_R8G8B8A8_UNORM) == false);
    static_assert(Engine::VkUtils::isDepthFormat(VK_FORMAT_UNDEFINED) == false);
}

// =============================================================================
// imageBarrier tests
// =============================================================================

class VkUtilsImageBarrierTest : public ::testing::Test {};

TEST_F(VkUtilsImageBarrierTest, SetsAllFieldsCorrectly)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xCAFE));
    VkImageSubresourceRange range {};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    range.baseMipLevel = 2;
    range.levelCount = 3;
    range.baseArrayLayer = 1;
    range.layerCount = 4;

    auto barrier = Engine::VkUtils::imageBarrier(
        fakeImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        range
    );

    EXPECT_EQ(barrier.sType, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
    EXPECT_EQ(barrier.image, fakeImage);
    EXPECT_EQ(barrier.oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.newLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    EXPECT_EQ(barrier.srcStageMask, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    EXPECT_EQ(barrier.srcAccessMask, VK_ACCESS_2_NONE);
    EXPECT_EQ(barrier.dstStageMask, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    EXPECT_EQ(barrier.dstAccessMask, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);

    // Subresource range
    EXPECT_EQ(barrier.subresourceRange.aspectMask, VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(barrier.subresourceRange.baseMipLevel, 2u);
    EXPECT_EQ(barrier.subresourceRange.levelCount, 3u);
    EXPECT_EQ(barrier.subresourceRange.baseArrayLayer, 1u);
    EXPECT_EQ(barrier.subresourceRange.layerCount, 4u);
}

TEST_F(VkUtilsImageBarrierTest, UsesDefaultSubresourceRange)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1234));

    auto barrier = Engine::VkUtils::imageBarrier(
        fakeImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    // Default range: {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    EXPECT_EQ(barrier.subresourceRange.aspectMask, VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(barrier.subresourceRange.baseMipLevel, 0u);
    EXPECT_EQ(barrier.subresourceRange.levelCount, 1u);
    EXPECT_EQ(barrier.subresourceRange.baseArrayLayer, 0u);
    EXPECT_EQ(barrier.subresourceRange.layerCount, 1u);
}

TEST_F(VkUtilsImageBarrierTest, QueueFamilyAlwaysIgnored)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xABCD));

    auto barrier = Engine::VkUtils::imageBarrier(
        fakeImage,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
}

// =============================================================================
// bufferBarrier tests
// =============================================================================

class VkUtilsBufferBarrierTest : public ::testing::Test {};

TEST_F(VkUtilsBufferBarrierTest, SetsAllFieldsCorrectly)
{
    VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xBEEF));

    auto barrier = Engine::VkUtils::bufferBarrier(
        fakeBuffer,
        128,
        1024,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    EXPECT_EQ(barrier.sType, VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2);
    EXPECT_EQ(barrier.buffer, fakeBuffer);
    EXPECT_EQ(barrier.offset, 128u);
    EXPECT_EQ(barrier.size, 1024u);
    EXPECT_EQ(barrier.srcStageMask, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(barrier.srcAccessMask, VK_ACCESS_2_SHADER_WRITE_BIT);
    EXPECT_EQ(barrier.dstStageMask, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
    EXPECT_EQ(barrier.dstAccessMask, VK_ACCESS_2_SHADER_READ_BIT);
    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
}

TEST_F(VkUtilsBufferBarrierTest, WholeBufferSize)
{
    VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x9999));

    auto barrier = Engine::VkUtils::bufferBarrier(
        fakeBuffer,
        0,
        VK_WHOLE_SIZE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    EXPECT_EQ(barrier.offset, 0u);
    EXPECT_EQ(barrier.size, VK_WHOLE_SIZE);
}

TEST_F(VkUtilsBufferBarrierTest, QueueFamilyAlwaysIgnored)
{
    VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x5555));

    auto barrier = Engine::VkUtils::bufferBarrier(
        fakeBuffer, 0, 512,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE
    );

    EXPECT_EQ(barrier.srcQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(barrier.dstQueueFamilyIndex, VK_QUEUE_FAMILY_IGNORED);
}
