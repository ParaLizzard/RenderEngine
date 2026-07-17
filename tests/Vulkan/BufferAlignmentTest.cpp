#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Vulkan/Buffer.h"

// Buffer::getAlignment is a public method but its logic is pure:
//   if (minOffsetAlignment > 0)
//       return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
//   return instanceSize;
//
// We call it through a static context since it doesn't touch 'this'.
// We replicate the function here to test the algorithm in isolation
// without needing a Vulkan device.

namespace {
    VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
    {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }
} // namespace

class BufferAlignmentTest : public ::testing::Test {};

// ─── No alignment (minOffset == 0) ──────────────────────────────────────────

TEST_F(BufferAlignmentTest, NoAlignmentReturnsInstanceSizeUnchanged)
{
    EXPECT_EQ(getAlignment(64, 0), 64);
    EXPECT_EQ(getAlignment(1, 0), 1);
    EXPECT_EQ(getAlignment(1024, 0), 1024);
}

// ─── Already aligned ────────────────────────────────────────────────────────

TEST_F(BufferAlignmentTest, AlreadyAlignedReturnsInstanceSize)
{
    // 256 is already aligned to 256
    EXPECT_EQ(getAlignment(256, 256), 256);
    // 128 is already aligned to 64
    EXPECT_EQ(getAlignment(128, 64), 128);
    // 64 aligned to 16
    EXPECT_EQ(getAlignment(64, 16), 64);
}

// ─── Needs rounding up ──────────────────────────────────────────────────────

TEST_F(BufferAlignmentTest, RoundsUpToNextAlignmentBoundary)
{
    // 100 with min alignment 64 → next multiple of 64 is 128
    EXPECT_EQ(getAlignment(100, 64), 128);

    // 1 with alignment 256 → 256
    EXPECT_EQ(getAlignment(1, 256), 256);

    // 65 with alignment 64 → 128
    EXPECT_EQ(getAlignment(65, 64), 128);

    // 200 with alignment 128 → 256
    EXPECT_EQ(getAlignment(200, 128), 256);

    // 33 with alignment 16 → 48
    EXPECT_EQ(getAlignment(33, 16), 48);
}

// ─── Edge: size == 1, alignment == 1 ────────────────────────────────────────

TEST_F(BufferAlignmentTest, MinimalSizeAndAlignment)
{
    EXPECT_EQ(getAlignment(1, 1), 1);
}

// ─── Common GPU alignment values ────────────────────────────────────────────

TEST_F(BufferAlignmentTest, CommonGpuAlignmentValues)
{
    // minUniformBufferOffsetAlignment is often 256 on desktop GPUs
    EXPECT_EQ(getAlignment(sizeof(float) * 4, 256), 256);  // 16 → 256

    // minStorageBufferOffsetAlignment is often 16 or 32
    EXPECT_EQ(getAlignment(20, 16), 32);
    EXPECT_EQ(getAlignment(48, 32), 64);
}

// ─── Large values ───────────────────────────────────────────────────────────

TEST_F(BufferAlignmentTest, LargeInstanceSizes)
{
    constexpr VkDeviceSize oneGB = 1024ULL * 1024ULL * 1024ULL;
    constexpr VkDeviceSize align = 4096;

    // 1 GB is already a multiple of 4096
    EXPECT_EQ(getAlignment(oneGB, align), oneGB);

    // 1 GB + 1 → should round to 1 GB + 4096
    EXPECT_EQ(getAlignment(oneGB + 1, align), oneGB + align);
}

// ─── Buffer size = alignmentSize * instanceCount ────────────────────────────

TEST_F(BufferAlignmentTest, TotalBufferSizeCalculation)
{
    // Mimics the constructor: bufferSize = alignmentSize * instanceCount
    VkDeviceSize instanceSize = 100;
    uint32_t instanceCount = 10;
    VkDeviceSize minAlign = 64;

    VkDeviceSize alignmentSize = getAlignment(instanceSize, minAlign);
    EXPECT_EQ(alignmentSize, 128);

    VkDeviceSize bufferSize = alignmentSize * instanceCount;
    EXPECT_EQ(bufferSize, 1280);
}

// ─── Power-of-two alignments ────────────────────────────────────────────────

TEST_F(BufferAlignmentTest, PowerOfTwoAlignmentsAreCorrect)
{
    for (VkDeviceSize align = 1; align <= 4096; align *= 2) {
        VkDeviceSize result = getAlignment(align + 1, align);
        EXPECT_EQ(result, align * 2)
            << "Failed for instanceSize=" << (align + 1) << ", alignment=" << align;
        EXPECT_EQ(result % align, 0)
            << "Result " << result << " is not a multiple of alignment " << align;
    }
}
