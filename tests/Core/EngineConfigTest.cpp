#include <gtest/gtest.h>
#include "Core/EngineConfig.h"

class EngineConfigTest : public ::testing::Test {};

// =============================================================================
// Value assertions
// =============================================================================

TEST_F(EngineConfigTest, MaxSceneObjectsIs100000)
{
    EXPECT_EQ(Engine::Config::MAX_SCENE_OBJECTS, 100'000u);
}

TEST_F(EngineConfigTest, MaxTexturesIs4096)
{
    EXPECT_EQ(Engine::Config::MAX_TEXTURES, 4096u);
}

TEST_F(EngineConfigTest, MaxFramesInFlightIs3)
{
    EXPECT_EQ(Engine::Config::MAX_FRAMES_IN_FLIGHT, 3u);
}

// =============================================================================
// Constexpr verification (compile-time usability)
// =============================================================================

TEST_F(EngineConfigTest, ValuesAreConstexpr)
{
    // These static_asserts verify the values are usable at compile time
    static_assert(Engine::Config::MAX_SCENE_OBJECTS == 100'000);
    static_assert(Engine::Config::MAX_TEXTURES == 4096);
    static_assert(Engine::Config::MAX_FRAMES_IN_FLIGHT == 3);
}

// =============================================================================
// Type correctness
// =============================================================================

TEST_F(EngineConfigTest, MaxSceneObjectsIsUint32)
{
    static_assert(std::is_same_v<decltype(Engine::Config::MAX_SCENE_OBJECTS), const std::uint32_t>);
}

TEST_F(EngineConfigTest, MaxTexturesIsUint32)
{
    static_assert(std::is_same_v<decltype(Engine::Config::MAX_TEXTURES), const uint32_t>);
}

TEST_F(EngineConfigTest, MaxFramesInFlightIsSizeT)
{
    static_assert(std::is_same_v<decltype(Engine::Config::MAX_FRAMES_IN_FLIGHT), const size_t>);
}

// =============================================================================
// Sanity bounds
// =============================================================================

TEST_F(EngineConfigTest, MaxSceneObjectsIsPositive)
{
    EXPECT_GT(Engine::Config::MAX_SCENE_OBJECTS, 0u);
}

TEST_F(EngineConfigTest, MaxTexturesIsPositive)
{
    EXPECT_GT(Engine::Config::MAX_TEXTURES, 0u);
}

TEST_F(EngineConfigTest, MaxFramesInFlightIsAtLeast2)
{
    // Triple buffering or double buffering — at least 2 frames
    EXPECT_GE(Engine::Config::MAX_FRAMES_IN_FLIGHT, 2u);
}

TEST_F(EngineConfigTest, MaxTexturesIsPowerOfTwo)
{
    uint32_t val = Engine::Config::MAX_TEXTURES;
    EXPECT_EQ(val & (val - 1), 0u) << "MAX_TEXTURES should be a power of two";
}

TEST_F(EngineConfigTest, MaxFramesInFlightIsReasonable)
{
    // Typically 2-4 for real-time rendering
    EXPECT_LE(Engine::Config::MAX_FRAMES_IN_FLIGHT, 8u);
}

// =============================================================================
// Can be used in array sizing
// =============================================================================

TEST_F(EngineConfigTest, CanSizeArrays)
{
    // Verify the constants can be used to size arrays (constexpr + integral)
    int frameArray[Engine::Config::MAX_FRAMES_IN_FLIGHT] = {};
    EXPECT_EQ(sizeof(frameArray), Engine::Config::MAX_FRAMES_IN_FLIGHT * sizeof(int));
}
