#include <gtest/gtest.h>
#include <cstdint>
#include <cstddef>
#include "Core/EngineConstants.h"

class EngineConstantsTest : public ::testing::Test {};

// =============================================================================
// 1. Compile-Time Invariants & Constexpr Validation
// =============================================================================

TEST_F(EngineConstantsTest, ValuesAreConstexpr)
{
    static_assert(Engine::Constants::MAX_FRAMES_IN_FLIGHT == 3);
    static_assert(Engine::Constants::MAX_SCENE_OBJECTS == 8192);
    static_assert(Engine::Constants::MAX_TEXTURES == 4096);
    static_assert(Engine::Constants::MAX_SAMPLERS == 64);
    static_assert(Engine::Constants::MAX_LIGHTS == 1024);
    static_assert(Engine::Constants::MAX_SHADOW_CASCADES == 3);

    static_assert(Engine::Constants::INVALID_INDEX == 0xFFFFFFFFu);
    static_assert(Engine::Constants::INVALID_BINDLESS_SLOT == 0xFFFFFFFFu);
    static_assert(Engine::Constants::INVALID_GPU_ADDRESS == 0ull);

    static_assert(Engine::Constants::PUSH_CONSTANT_MAX_SIZE == 128);
    static_assert(Engine::Constants::BDA_BUFFER_ALIGNMENT == 16);
    static_assert(Engine::Constants::CACHELINE_SIZE == 64);

    static_assert(Engine::Constants::MAX_MESHLET_VERTICES == 64);
    static_assert(Engine::Constants::MAX_MESHLET_TRIANGLES == 124);
    static_assert(Engine::Constants::CONE_WEIGHT == 0.5f);

    static_assert(Engine::Constants::CULL_WORKGROUP_SIZE == 256);
    static_assert(Engine::Constants::HIZ_MIP_WORKGROUP_SIZE == 16);
    static_assert(Engine::Constants::MATERIAL_WORKGROUP_SIZE == 8);
    static_assert(Engine::Constants::SSAO_WORKGROUP_SIZE == 8);

    static_assert(Engine::Constants::WARMUP_DELAY_SECONDS == 30.0f);
    static_assert(Engine::Constants::MAX_TIMESTAMP_QUERIES == 128);
}

// =============================================================================
// 2. Hardware Bounds and Sanity Checks
// =============================================================================

TEST_F(EngineConstantsTest, FrameBufferingBounds)
{
    EXPECT_GE(Engine::Constants::MAX_FRAMES_IN_FLIGHT, 2u);
    EXPECT_LE(Engine::Constants::MAX_FRAMES_IN_FLIGHT, 4u);
}

TEST_F(EngineConstantsTest, PowerOfTwoSizes)
{
    auto isPowerOfTwo = [](uint64_t val) {
        return val > 0 && (val & (val - 1)) == 0;
    };

    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::MAX_SCENE_OBJECTS));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::MAX_TEXTURES));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::MAX_SAMPLERS));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::MAX_LIGHTS));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::PUSH_CONSTANT_MAX_SIZE));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::BDA_BUFFER_ALIGNMENT));
    EXPECT_TRUE(isPowerOfTwo(Engine::Constants::CACHELINE_SIZE));
}

// =============================================================================
// 3. GPU Workgroup & Warp/Wavefront Alignment
// =============================================================================

TEST_F(EngineConstantsTest, WorkgroupAlignment)
{
    // CULL_WORKGROUP_SIZE must be a multiple of 32/64 for NVIDIA warp / AMD wavefront efficiency
    EXPECT_EQ(Engine::Constants::CULL_WORKGROUP_SIZE % 32, 0u);
    EXPECT_EQ(Engine::Constants::CULL_WORKGROUP_SIZE % 64, 0u);

    // 2D compute workgroups (e.g. 8x8 or 16x16) total invocation count must be >= 32
    EXPECT_GE(Engine::Constants::HIZ_MIP_WORKGROUP_SIZE * Engine::Constants::HIZ_MIP_WORKGROUP_SIZE, 32u);
    EXPECT_GE(Engine::Constants::MATERIAL_WORKGROUP_SIZE * Engine::Constants::MATERIAL_WORKGROUP_SIZE, 32u);
    EXPECT_GE(Engine::Constants::SSAO_WORKGROUP_SIZE * Engine::Constants::SSAO_WORKGROUP_SIZE, 32u);
}

// =============================================================================
// 4. Meshlet Pipeline Constraints
// =============================================================================

TEST_F(EngineConstantsTest, MeshletHardwareLimits)
{
    // NV/AMD mesh shader spec: max_vertices <= 256, max_primitives <= 256
    EXPECT_LE(Engine::Constants::MAX_MESHLET_VERTICES, 64u);
    EXPECT_LE(Engine::Constants::MAX_MESHLET_TRIANGLES, 124u);

    EXPECT_GT(Engine::Constants::CONE_WEIGHT, 0.0f);
    EXPECT_LE(Engine::Constants::CONE_WEIGHT, 1.0f);
}

// =============================================================================
// 5. Array Sizing Compile-Time Check
// =============================================================================

TEST_F(EngineConstantsTest, ArraySizingAndAlignment)
{
    int frameArray[Engine::Constants::MAX_FRAMES_IN_FLIGHT] = {};
    EXPECT_EQ(sizeof(frameArray), Engine::Constants::MAX_FRAMES_IN_FLIGHT * sizeof(int));

    alignas(Engine::Constants::Hardware::BDA_BUFFER_ALIGNMENT) uint8_t bdaAlignedBuffer[64];
    EXPECT_EQ(reinterpret_cast<uintptr_t>(bdaAlignedBuffer) % Engine::Constants::Hardware::BDA_BUFFER_ALIGNMENT, 0u);
}
