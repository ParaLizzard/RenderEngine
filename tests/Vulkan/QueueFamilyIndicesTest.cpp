#include <gtest/gtest.h>
#include "Vulkan/VulkanDevice.h"

class QueueFamilyIndicesTest : public ::testing::Test {};

TEST_F(QueueFamilyIndicesTest, DefaultIsNotComplete)
{
    Engine::QueueFamilyIndices indices{};
    EXPECT_FALSE(indices.IsComplete());
    EXPECT_EQ(indices.graphicsFamily, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(indices.computeFamily, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(indices.transferFamily, VK_QUEUE_FAMILY_IGNORED);
    EXPECT_EQ(indices.presentFamily, VK_QUEUE_FAMILY_IGNORED);
}

TEST_F(QueueFamilyIndicesTest, PartialCompletionIsNotComplete)
{
    Engine::QueueFamilyIndices indices{};
    indices.graphicsFamily = 0;
    EXPECT_FALSE(indices.IsComplete());

    indices.computeFamily = 1;
    EXPECT_FALSE(indices.IsComplete());

    indices.transferFamily = 2;
    EXPECT_FALSE(indices.IsComplete());

    indices.presentFamily = 0;
    EXPECT_TRUE(indices.IsComplete());
}

TEST_F(QueueFamilyIndicesTest, AllSetIsComplete)
{
    Engine::QueueFamilyIndices indices{};
    indices.graphicsFamily = 0;
    indices.computeFamily = 1;
    indices.transferFamily = 2;
    indices.presentFamily = 3;

    EXPECT_TRUE(indices.IsComplete());
}

TEST_F(QueueFamilyIndicesTest, SharedFamilyIndicesStillComplete)
{
    Engine::QueueFamilyIndices indices{};
    indices.graphicsFamily = 0;
    indices.computeFamily = 0;
    indices.transferFamily = 0;
    indices.presentFamily = 0;

    EXPECT_TRUE(indices.IsComplete());
}

TEST_F(QueueFamilyIndicesTest, DedicatedQueueChecks)
{
    // Case 1: Shared queue family for everything
    Engine::QueueFamilyIndices shared{};
    shared.graphicsFamily = 0;
    shared.computeFamily = 0;
    shared.transferFamily = 0;
    shared.presentFamily = 0;

    EXPECT_FALSE(shared.IsComputeDedicated());
    EXPECT_FALSE(shared.IsTransferDedicated());
    EXPECT_FALSE(shared.IsTransferAsync());

    // Case 2: Dedicated compute and dedicated transfer
    Engine::QueueFamilyIndices dedicated{};
    dedicated.graphicsFamily = 0;
    dedicated.computeFamily = 1;
    dedicated.transferFamily = 2;
    dedicated.presentFamily = 0;

    EXPECT_TRUE(dedicated.IsComputeDedicated());
    EXPECT_TRUE(dedicated.IsTransferDedicated());
    EXPECT_TRUE(dedicated.IsTransferAsync());

    // Case 3: Transfer shares family with compute, but differs from graphics
    Engine::QueueFamilyIndices asyncOnly{};
    asyncOnly.graphicsFamily = 0;
    asyncOnly.computeFamily = 1;
    asyncOnly.transferFamily = 1;
    asyncOnly.presentFamily = 0;

    EXPECT_TRUE(asyncOnly.IsComputeDedicated());
    EXPECT_FALSE(asyncOnly.IsTransferDedicated());
    EXPECT_TRUE(asyncOnly.IsTransferAsync());
}
