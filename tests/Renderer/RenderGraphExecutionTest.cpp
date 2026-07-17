#include <gtest/gtest.h>
#include "Renderer/RenderGraph.h"

// Tests for the execute() method's barrier generation and state tracking logic.
// Since execute() calls vkCmdPipelineBarrier2 (a real Vulkan call), we can't
// actually call execute() without a valid VkCommandBuffer. Instead, we test
// the state-tracking DATA STRUCTURES that drive barrier decisions.
//
// The key logic in execute() is:
//   for each pass:
//     for each image usage:
//       if layout or access changed → emit barrier, update GraphImage state
//     for each buffer usage:
//       if access changed → emit barrier, update GraphBuffer state
//
// We verify this by testing GraphImage/GraphBuffer struct state transitions.

namespace {
    using namespace Engine;

    // Simulates the barrier-decision logic from RenderGraph::execute()
    struct BarrierDecision {
        bool layoutChanged;
        bool accessChanged;
        bool shouldBarrier;
    };

    BarrierDecision shouldEmitImageBarrier(const GraphImage &g, const ImageUsageDeclaration &decl)
    {
        bool layoutChanged = g.layout != decl.imageLayout;
        bool accessChanged = g.lastAccessMask != decl.accessMask || g.lastStageMask != decl.stageMask;
        return {layoutChanged, accessChanged, layoutChanged || accessChanged};
    }

    void applyImageUsage(GraphImage &g, const ImageUsageDeclaration &decl)
    {
        g.layout = decl.imageLayout;
        g.lastStageMask = decl.stageMask;
        g.lastAccessMask = decl.accessMask;
    }

    bool shouldEmitBufferBarrier(const GraphBuffer &g, const BufferUsageDeclaration &decl)
    {
        return g.lastAccessMask != decl.accessMask || g.lastStageMask != decl.stageMask;
    }

    void applyBufferUsage(GraphBuffer &g, const BufferUsageDeclaration &decl)
    {
        g.lastStageMask = decl.stageMask;
        g.lastAccessMask = decl.accessMask;
    }
} // namespace

class RenderGraphExecutionTest : public ::testing::Test {};

// =============================================================================
// Image: layout change triggers barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, LayoutChangeTriggerImageBarrier)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    img.lastAccessMask = VK_ACCESS_2_NONE;

    ImageUsageDeclaration decl {
        "color",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        ResourceUsageType::Write
    };

    auto decision = shouldEmitImageBarrier(img, decl);
    EXPECT_TRUE(decision.layoutChanged);
    EXPECT_TRUE(decision.shouldBarrier);
}

// =============================================================================
// Image: same layout but different access triggers barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, SameLayoutDifferentAccessTriggersBarrier)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_GENERAL;
    img.lastStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    img.lastAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    ImageUsageDeclaration decl {
        "storage",
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    auto decision = shouldEmitImageBarrier(img, decl);
    EXPECT_FALSE(decision.layoutChanged);
    EXPECT_TRUE(decision.accessChanged);
    EXPECT_TRUE(decision.shouldBarrier);
}

// =============================================================================
// Image: identical state → no barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, IdenticalImageStateNoBarrier)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.lastStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    img.lastAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    ImageUsageDeclaration decl {
        "texture",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    auto decision = shouldEmitImageBarrier(img, decl);
    EXPECT_FALSE(decision.shouldBarrier);
}

// =============================================================================
// Image: state is updated after apply
// =============================================================================

TEST_F(RenderGraphExecutionTest, ImageStateUpdatedAfterApply)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    img.lastAccessMask = VK_ACCESS_2_NONE;

    ImageUsageDeclaration decl {
        "color",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        ResourceUsageType::Write
    };

    applyImageUsage(img, decl);

    EXPECT_EQ(img.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(img.lastStageMask, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(img.lastAccessMask, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}

// =============================================================================
// Multi-pass chain: Write → Read transition
// =============================================================================

TEST_F(RenderGraphExecutionTest, MultiPassWriteThenReadTransition)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    img.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    img.lastAccessMask = VK_ACCESS_2_NONE;

    // Pass 1: Write
    ImageUsageDeclaration writeDecl {
        "target",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        ResourceUsageType::Write
    };

    EXPECT_TRUE(shouldEmitImageBarrier(img, writeDecl).shouldBarrier);
    applyImageUsage(img, writeDecl);

    // Pass 2: Read (should need a barrier for layout + access change)
    ImageUsageDeclaration readDecl {
        "target",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    auto decision = shouldEmitImageBarrier(img, readDecl);
    EXPECT_TRUE(decision.layoutChanged);
    EXPECT_TRUE(decision.accessChanged);
    EXPECT_TRUE(decision.shouldBarrier);

    applyImageUsage(img, readDecl);
    EXPECT_EQ(img.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// =============================================================================
// Multi-pass chain: Read → Read (same stage) → no barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, ConsecutiveIdenticalReadsNoBarrier)
{
    GraphImage img {};
    img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.lastStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    img.lastAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    ImageUsageDeclaration readDecl {
        "texture",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    EXPECT_FALSE(shouldEmitImageBarrier(img, readDecl).shouldBarrier);
    applyImageUsage(img, readDecl);
    EXPECT_FALSE(shouldEmitImageBarrier(img, readDecl).shouldBarrier);
}

// =============================================================================
// Buffer: access change triggers barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, BufferAccessChangeTriggerBarrier)
{
    GraphBuffer buf {};
    buf.lastStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    buf.lastAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    BufferUsageDeclaration decl {
        "ssbo",
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    EXPECT_TRUE(shouldEmitBufferBarrier(buf, decl));
}

// =============================================================================
// Buffer: identical state → no barrier
// =============================================================================

TEST_F(RenderGraphExecutionTest, IdenticalBufferStateNoBarrier)
{
    GraphBuffer buf {};
    buf.lastStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    buf.lastAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    BufferUsageDeclaration decl {
        "ssbo",
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        ResourceUsageType::Read
    };

    EXPECT_FALSE(shouldEmitBufferBarrier(buf, decl));
}

// =============================================================================
// Buffer: state updated after apply
// =============================================================================

TEST_F(RenderGraphExecutionTest, BufferStateUpdatedAfterApply)
{
    GraphBuffer buf {};
    buf.lastStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    buf.lastAccessMask = VK_ACCESS_2_NONE;

    BufferUsageDeclaration decl {
        "indirect",
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        ResourceUsageType::Read
    };

    applyBufferUsage(buf, decl);

    EXPECT_EQ(buf.lastStageMask, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    EXPECT_EQ(buf.lastAccessMask, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
}

// =============================================================================
// GraphImage default initial state
// =============================================================================

TEST_F(RenderGraphExecutionTest, GraphImageDefaultState)
{
    GraphImage img {};
    EXPECT_EQ(img.image, VK_NULL_HANDLE);
    EXPECT_EQ(img.imageView, VK_NULL_HANDLE);
    EXPECT_EQ(img.imageFormat, VK_FORMAT_UNDEFINED);
    EXPECT_EQ(img.extent.width, 0u);
    EXPECT_EQ(img.extent.height, 0u);
    EXPECT_EQ(img.layout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(img.lastStageMask, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    EXPECT_EQ(img.lastAccessMask, VK_ACCESS_2_NONE);
    EXPECT_EQ(img.mipLevels, 1u);
    EXPECT_EQ(img.arrayLayers, 1u);
}

// =============================================================================
// GraphBuffer default initial state
// =============================================================================

TEST_F(RenderGraphExecutionTest, GraphBufferDefaultState)
{
    GraphBuffer buf {};
    EXPECT_EQ(buf.buffer, VK_NULL_HANDLE);
    EXPECT_EQ(buf.size, 0u);
    EXPECT_EQ(buf.lastStageMask, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    EXPECT_EQ(buf.lastAccessMask, VK_ACCESS_2_NONE);
}
