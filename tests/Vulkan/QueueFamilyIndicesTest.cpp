#include <gtest/gtest.h>
#include "Vulkan/Device.h"

class QueueFamilyIndicesTest : public ::testing::Test {};

TEST_F(QueueFamilyIndicesTest, DefaultIsNotComplete)
{
    Engine::QueueFamilyIndices indices {};
    EXPECT_FALSE(indices.isComplete());
    EXPECT_FALSE(indices.graphicsFamilyHasValue);
    EXPECT_FALSE(indices.presentFamilyHasValue);
}

TEST_F(QueueFamilyIndicesTest, OnlyGraphicsSetIsNotComplete)
{
    Engine::QueueFamilyIndices indices {};
    indices.graphicsFamily = 0;
    indices.graphicsFamilyHasValue = true;

    EXPECT_FALSE(indices.isComplete());
}

TEST_F(QueueFamilyIndicesTest, OnlyPresentSetIsNotComplete)
{
    Engine::QueueFamilyIndices indices {};
    indices.presentFamily = 1;
    indices.presentFamilyHasValue = true;

    EXPECT_FALSE(indices.isComplete());
}

TEST_F(QueueFamilyIndicesTest, BothSetIsComplete)
{
    Engine::QueueFamilyIndices indices {};
    indices.graphicsFamily = 0;
    indices.graphicsFamilyHasValue = true;
    indices.presentFamily = 0;
    indices.presentFamilyHasValue = true;

    EXPECT_TRUE(indices.isComplete());
}

TEST_F(QueueFamilyIndicesTest, DifferentFamilyIndicesStillComplete)
{
    Engine::QueueFamilyIndices indices {};
    indices.graphicsFamily = 0;
    indices.graphicsFamilyHasValue = true;
    indices.presentFamily = 2;
    indices.presentFamilyHasValue = true;

    EXPECT_TRUE(indices.isComplete());
    EXPECT_NE(indices.graphicsFamily, indices.presentFamily);
}

TEST_F(QueueFamilyIndicesTest, SameFamilyIndexForBothQueues)
{
    Engine::QueueFamilyIndices indices {};
    indices.graphicsFamily = 3;
    indices.graphicsFamilyHasValue = true;
    indices.presentFamily = 3;
    indices.presentFamilyHasValue = true;

    EXPECT_TRUE(indices.isComplete());
    EXPECT_EQ(indices.graphicsFamily, indices.presentFamily);
}
