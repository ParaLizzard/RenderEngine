#include <gtest/gtest.h>
#include "Renderer/FrameInfo.h"

class FrameInfoStructTest : public ::testing::Test {};

// =============================================================================
// Default initialization
// =============================================================================

TEST_F(FrameInfoStructTest, DefaultPointersAreNull)
{
    Engine::FrameInfo info {};

    EXPECT_EQ(info.device, nullptr);
    EXPECT_EQ(info.renderGraph, nullptr);
    EXPECT_EQ(info.renderer, nullptr);
    EXPECT_EQ(info.megaBuffer, nullptr);
    EXPECT_EQ(info.resourceHeap, nullptr);
}

TEST_F(FrameInfoStructTest, DefaultEnableSSAOIsTrue)
{
    Engine::FrameInfo info {};
    EXPECT_TRUE(info.enableSSAO);
}

TEST_F(FrameInfoStructTest, DefaultNumericFieldsAreZero)
{
    Engine::FrameInfo info {};
    info.frameIndex = 0;
    info.frameTime = 0.0f;

    EXPECT_EQ(info.frameIndex, 0);
    EXPECT_FLOAT_EQ(info.frameTime, 0.0f);
}

// =============================================================================
// SceneUbo struct
// =============================================================================

TEST_F(FrameInfoStructTest, SceneUboSizeIsReasonable)
{
    // SceneUbo should be a reasonable GPU-compatible size
    // Contains: vec4 + vec4 + float + uint32 + vec2 = 16+16+4+4+8 = 48 bytes minimum
    EXPECT_GE(sizeof(Engine::SceneUbo), 48);
}

TEST_F(FrameInfoStructTest, SceneUboDefaultValues)
{
    Engine::SceneUbo ubo {};

    // Verify default zero-initialized
    EXPECT_FLOAT_EQ(ubo.cameraPosition.x, 0.0f);
    EXPECT_FLOAT_EQ(ubo.cameraPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(ubo.cameraPosition.z, 0.0f);
    EXPECT_FLOAT_EQ(ubo.cameraPosition.w, 0.0f);

    EXPECT_FLOAT_EQ(ubo.directionalLight.x, 0.0f);
    EXPECT_FLOAT_EQ(ubo.directionalLight.y, 0.0f);
    EXPECT_FLOAT_EQ(ubo.directionalLight.z, 0.0f);
    EXPECT_FLOAT_EQ(ubo.directionalLight.w, 0.0f);

    EXPECT_FLOAT_EQ(ubo.maxReflectionLod, 0.0f);
    EXPECT_EQ(ubo.blueNoiseTexIndex, 0u);
}

TEST_F(FrameInfoStructTest, SceneUboFieldAssignment)
{
    Engine::SceneUbo ubo {};
    ubo.cameraPosition = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
    ubo.directionalLight = glm::vec4(0.0f, -1.0f, 0.5f, 0.0f);
    ubo.maxReflectionLod = 5.0f;
    ubo.blueNoiseTexIndex = 42;

    EXPECT_FLOAT_EQ(ubo.cameraPosition.x, 1.0f);
    EXPECT_FLOAT_EQ(ubo.cameraPosition.y, 2.0f);
    EXPECT_FLOAT_EQ(ubo.cameraPosition.z, 3.0f);
    EXPECT_FLOAT_EQ(ubo.directionalLight.y, -1.0f);
    EXPECT_FLOAT_EQ(ubo.maxReflectionLod, 5.0f);
    EXPECT_EQ(ubo.blueNoiseTexIndex, 42u);
}

// =============================================================================
// FrameData struct (from Renderer.h)
// =============================================================================

TEST_F(FrameInfoStructTest, FrameInfoCanStoreExtent)
{
    Engine::FrameInfo info {};
    info.extent = {1920, 1080};

    EXPECT_EQ(info.extent.width, 1920u);
    EXPECT_EQ(info.extent.height, 1080u);
}

TEST_F(FrameInfoStructTest, FrameInfoCanBeModified)
{
    Engine::FrameInfo info {};
    info.frameIndex = 2;
    info.frameTime = 0.016f;
    info.enableSSAO = false;

    EXPECT_EQ(info.frameIndex, 2);
    EXPECT_FLOAT_EQ(info.frameTime, 0.016f);
    EXPECT_FALSE(info.enableSSAO);
}
