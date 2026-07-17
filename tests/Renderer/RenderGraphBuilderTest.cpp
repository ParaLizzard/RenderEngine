#include <gtest/gtest.h>
#include "Renderer/RenderGraph.h"

// RenderGraphBuilder directly populates vectors without any Vulkan calls,
// so we can test it fully in isolation.

class RenderGraphBuilderTest : public ::testing::Test {
protected:
    std::vector<Engine::ImageUsageDeclaration> imageUsages;
    std::vector<Engine::TransientImageDeclaration> transientImages;
    std::vector<Engine::BufferUsageDeclaration> bufferUsages;

    Engine::RenderGraphBuilder createBuilder()
    {
        return Engine::RenderGraphBuilder(imageUsages, transientImages, bufferUsages);
    }
};

// =============================================================================
// readImage
// =============================================================================

TEST_F(RenderGraphBuilderTest, ReadImagePushesCorrectDeclaration)
{
    auto builder = createBuilder();
    builder.readImage(
        "colorTarget",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    ASSERT_EQ(imageUsages.size(), 1);
    EXPECT_EQ(imageUsages[0].imageName, "colorTarget");
    EXPECT_EQ(imageUsages[0].imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(imageUsages[0].stageMask, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(imageUsages[0].accessMask, VK_ACCESS_2_SHADER_READ_BIT);
    EXPECT_EQ(imageUsages[0].usageType, Engine::ResourceUsageType::Read);
}

// =============================================================================
// writeImage
// =============================================================================

TEST_F(RenderGraphBuilderTest, WriteImagePushesCorrectDeclaration)
{
    auto builder = createBuilder();
    builder.writeImage(
        "gbufferNormal",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    ASSERT_EQ(imageUsages.size(), 1);
    EXPECT_EQ(imageUsages[0].imageName, "gbufferNormal");
    EXPECT_EQ(imageUsages[0].imageLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(imageUsages[0].stageMask, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(imageUsages[0].accessMask, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    EXPECT_EQ(imageUsages[0].usageType, Engine::ResourceUsageType::Write);
}

// =============================================================================
// readBuffer
// =============================================================================

TEST_F(RenderGraphBuilderTest, ReadBufferPushesCorrectDeclaration)
{
    auto builder = createBuilder();
    builder.readBuffer(
        "objectSSBO",
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    ASSERT_EQ(bufferUsages.size(), 1);
    EXPECT_EQ(bufferUsages[0].bufferName, "objectSSBO");
    EXPECT_EQ(bufferUsages[0].stageMask, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
    EXPECT_EQ(bufferUsages[0].accessMask, VK_ACCESS_2_SHADER_READ_BIT);
    EXPECT_EQ(bufferUsages[0].usageType, Engine::ResourceUsageType::Read);
}

// =============================================================================
// writeBuffer
// =============================================================================

TEST_F(RenderGraphBuilderTest, WriteBufferPushesCorrectDeclaration)
{
    auto builder = createBuilder();
    builder.writeBuffer(
        "indirectCmds",
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT
    );

    ASSERT_EQ(bufferUsages.size(), 1);
    EXPECT_EQ(bufferUsages[0].bufferName, "indirectCmds");
    EXPECT_EQ(bufferUsages[0].stageMask, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(bufferUsages[0].accessMask, VK_ACCESS_2_SHADER_WRITE_BIT);
    EXPECT_EQ(bufferUsages[0].usageType, Engine::ResourceUsageType::Write);
}

// =============================================================================
// readWriteBuffer
// =============================================================================

TEST_F(RenderGraphBuilderTest, ReadWriteBufferPushesCorrectDeclaration)
{
    auto builder = createBuilder();
    builder.readWriteBuffer(
        "counterBuffer",
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
    );

    ASSERT_EQ(bufferUsages.size(), 1);
    EXPECT_EQ(bufferUsages[0].bufferName, "counterBuffer");
    EXPECT_EQ(bufferUsages[0].usageType, Engine::ResourceUsageType::ReadWrite);
}

// =============================================================================
// createTransientImage
// =============================================================================

TEST_F(RenderGraphBuilderTest, CreateTransientImagePushesCorrectDeclaration)
{
    VkExtent2D extent {1920, 1080};
    VkClearValue clear {};
    clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    auto builder = createBuilder();
    builder.createTransientImage(
        "ssaoOutput",
        VK_FORMAT_R8_UNORM,
        extent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        clear
    );

    ASSERT_EQ(transientImages.size(), 1);
    EXPECT_EQ(transientImages[0].name, "ssaoOutput");
    EXPECT_EQ(transientImages[0].format, VK_FORMAT_R8_UNORM);
    EXPECT_EQ(transientImages[0].extent.width, 1920u);
    EXPECT_EQ(transientImages[0].extent.height, 1080u);
    EXPECT_EQ(transientImages[0].usage,
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
}

TEST_F(RenderGraphBuilderTest, CreateTransientImageDefaultUsageFlags)
{
    auto builder = createBuilder();
    builder.createTransientImage(
        "depthBuffer",
        VK_FORMAT_D32_SFLOAT,
        {1920, 1080}
    );

    ASSERT_EQ(transientImages.size(), 1);
    // Default usage: COLOR_ATTACHMENT | SAMPLED
    EXPECT_EQ(transientImages[0].usage,
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
}

// =============================================================================
// Multiple calls accumulate correctly
// =============================================================================

TEST_F(RenderGraphBuilderTest, MultipleDeclsAccumulateInOrder)
{
    auto builder = createBuilder();

    builder.readImage("A", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    builder.writeImage("B", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    builder.readBuffer("C", VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    builder.writeBuffer("D", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    builder.createTransientImage("E", VK_FORMAT_R8G8B8A8_UNORM, {800, 600});

    EXPECT_EQ(imageUsages.size(), 2);
    EXPECT_EQ(imageUsages[0].imageName, "A");
    EXPECT_EQ(imageUsages[1].imageName, "B");

    EXPECT_EQ(bufferUsages.size(), 2);
    EXPECT_EQ(bufferUsages[0].bufferName, "C");
    EXPECT_EQ(bufferUsages[1].bufferName, "D");

    EXPECT_EQ(transientImages.size(), 1);
    EXPECT_EQ(transientImages[0].name, "E");
}

// =============================================================================
// Empty builder produces empty vectors
// =============================================================================

TEST_F(RenderGraphBuilderTest, EmptyBuilderProducesEmptyVectors)
{
    auto builder = createBuilder();
    // Don't add anything

    EXPECT_TRUE(imageUsages.empty());
    EXPECT_TRUE(transientImages.empty());
    EXPECT_TRUE(bufferUsages.empty());
}

// =============================================================================
// Duplicate names are allowed (graph tracks by order, not uniqueness)
// =============================================================================

TEST_F(RenderGraphBuilderTest, DuplicateNamesAreAllowed)
{
    auto builder = createBuilder();
    builder.readImage("shared", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    builder.writeImage("shared", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    EXPECT_EQ(imageUsages.size(), 2);
    EXPECT_EQ(imageUsages[0].usageType, Engine::ResourceUsageType::Read);
    EXPECT_EQ(imageUsages[1].usageType, Engine::ResourceUsageType::Write);
}
