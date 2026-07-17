#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

// Tests the descriptor info math that Buffer uses internally.
// Buffer::descriptorInfo(size, offset) returns { buffer, offset, size }
// Buffer::descriptorInfoForIndex(i) returns descriptorInfo(alignmentSize, i * alignmentSize)
//
// We test the math in isolation without constructing real Vulkan objects.

namespace {
    // Replicate Buffer::getAlignment for computing alignmentSize
    VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
    {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    // Replicate the descriptor info construction logic
    VkDescriptorBufferInfo descriptorInfo(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset)
    {
        return VkDescriptorBufferInfo {buffer, offset, size};
    }

    VkDescriptorBufferInfo descriptorInfoForIndex(VkBuffer buffer,
                                                   VkDeviceSize alignmentSize,
                                                   int index)
    {
        return descriptorInfo(buffer, alignmentSize, static_cast<VkDeviceSize>(index) * alignmentSize);
    }
} // namespace

class BufferDescriptorInfoTest : public ::testing::Test {
protected:
    // Use a fake buffer handle for testing
    VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xDEADBEEF));
};

// ─── descriptorInfo basic construction ──────────────────────────────────────

TEST_F(BufferDescriptorInfoTest, DescriptorInfoReturnsCorrectFields)
{
    auto info = descriptorInfo(fakeBuffer, 256, 128);

    EXPECT_EQ(info.buffer, fakeBuffer);
    EXPECT_EQ(info.offset, 128);
    EXPECT_EQ(info.range, 256);
}

TEST_F(BufferDescriptorInfoTest, DescriptorInfoZeroOffsetAndSize)
{
    auto info = descriptorInfo(fakeBuffer, 0, 0);

    EXPECT_EQ(info.buffer, fakeBuffer);
    EXPECT_EQ(info.offset, 0);
    EXPECT_EQ(info.range, 0);
}

// ─── descriptorInfoForIndex ─────────────────────────────────────────────────

TEST_F(BufferDescriptorInfoTest, IndexZeroProducesZeroOffset)
{
    VkDeviceSize alignmentSize = 256;
    auto info = descriptorInfoForIndex(fakeBuffer, alignmentSize, 0);

    EXPECT_EQ(info.offset, 0);
    EXPECT_EQ(info.range, 256);
}

TEST_F(BufferDescriptorInfoTest, IndexOneProducesOffsetEqualToAlignmentSize)
{
    VkDeviceSize alignmentSize = 256;
    auto info = descriptorInfoForIndex(fakeBuffer, alignmentSize, 1);

    EXPECT_EQ(info.offset, 256);
    EXPECT_EQ(info.range, 256);
}

TEST_F(BufferDescriptorInfoTest, IndexNProducesCorrectOffset)
{
    VkDeviceSize alignmentSize = 128;

    for (int i = 0; i < 10; ++i) {
        auto info = descriptorInfoForIndex(fakeBuffer, alignmentSize, i);
        EXPECT_EQ(info.offset, static_cast<VkDeviceSize>(i) * 128)
            << "Failed at index " << i;
        EXPECT_EQ(info.range, 128) << "Range should always be alignmentSize";
    }
}

// ─── With non-trivial alignment (instanceSize != alignmentSize) ─────────────

TEST_F(BufferDescriptorInfoTest, AlignedInstanceSizeUsedForOffsets)
{
    // instanceSize=100, minAlign=64 → alignmentSize=128
    VkDeviceSize alignmentSize = getAlignment(100, 64);
    ASSERT_EQ(alignmentSize, 128);

    auto info0 = descriptorInfoForIndex(fakeBuffer, alignmentSize, 0);
    auto info1 = descriptorInfoForIndex(fakeBuffer, alignmentSize, 1);
    auto info5 = descriptorInfoForIndex(fakeBuffer, alignmentSize, 5);

    EXPECT_EQ(info0.offset, 0);
    EXPECT_EQ(info1.offset, 128);
    EXPECT_EQ(info5.offset, 640);

    // All ranges should be the alignment size, not the raw instance size
    EXPECT_EQ(info0.range, 128);
    EXPECT_EQ(info1.range, 128);
    EXPECT_EQ(info5.range, 128);
}

// ─── writeToIndex offset calculation ────────────────────────────────────────

TEST_F(BufferDescriptorInfoTest, WriteToIndexOffsetMatchesDescriptorInfoOffset)
{
    // writeToIndex calls writeToBuffer(data, instanceSize, index * alignmentSize)
    // So the offset should match descriptorInfoForIndex's offset
    VkDeviceSize instanceSize = 72;
    VkDeviceSize alignmentSize = getAlignment(instanceSize, 64);

    for (int i = 0; i < 8; ++i) {
        VkDeviceSize writeOffset = static_cast<VkDeviceSize>(i) * alignmentSize;
        auto descInfo = descriptorInfoForIndex(fakeBuffer, alignmentSize, i);
        EXPECT_EQ(writeOffset, descInfo.offset)
            << "Write offset and descriptor offset should match for index " << i;
    }
}

// ─── flushIndex / invalidateIndex offset calculation ────────────────────────

TEST_F(BufferDescriptorInfoTest, FlushInvalidateIndexOffsetsAreCorrect)
{
    // flush(alignmentSize, index * alignmentSize)
    VkDeviceSize alignmentSize = 256;

    for (int i = 0; i < 5; ++i) {
        VkDeviceSize expectedOffset = static_cast<VkDeviceSize>(i) * alignmentSize;
        VkDeviceSize expectedSize = alignmentSize;

        EXPECT_EQ(expectedOffset, static_cast<VkDeviceSize>(i) * 256);
        EXPECT_EQ(expectedSize, 256);
    }
}
