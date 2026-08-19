#include <gtest/gtest.h>
#include "Common/SyntheticMeshGenerator.h"
#include "Common/SyntheticTextureGenerator.h"
#include "Common/TestUtils.h"

using namespace Engine::Test;
using namespace Engine::TestUtils;

// =============================================================================
// Synthetic Mesh Generator Unit Tests
// =============================================================================

TEST(SyntheticGeneratorsTest, GenerateCubeMesh) {
    auto mesh = SyntheticMeshGenerator::GenerateCube(1.0f);

    // Cube has 6 faces * 4 vertices = 24 vertices
    EXPECT_EQ(mesh.vertices.size(), 24u);
    // Cube has 6 faces * 2 triangles * 3 indices = 36 indices
    EXPECT_EQ(mesh.indices.size(), 36u);

    // Check AABB bounds
    ExpectNearVec3(mesh.aabbMin, glm::vec3(-1.0f));
    ExpectNearVec3(mesh.aabbMax, glm::vec3(1.0f));

    // Verify all indices are within vertex range
    for (uint32_t idx : mesh.indices) {
        EXPECT_LT(idx, mesh.vertices.size());
    }

    // Verify normals are unit length
    for (const auto& v : mesh.vertices) {
        EXPECT_NEAR(glm::length(v.normal), 1.0f, 1e-4f);
    }
}

TEST(SyntheticGeneratorsTest, GenerateQuadMesh) {
    auto quad = SyntheticMeshGenerator::GenerateQuad(2.0f);

    EXPECT_EQ(quad.vertices.size(), 4u);
    EXPECT_EQ(quad.indices.size(), 6u);

    for (const auto& v : quad.vertices) {
        ExpectNearVec3(v.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    }
}

TEST(SyntheticGeneratorsTest, GenerateSphereMesh) {
    auto sphere = SyntheticMeshGenerator::GenerateSphere(1.5f, 10, 10);

    EXPECT_GT(sphere.vertices.size(), 0u);
    EXPECT_GT(sphere.indices.size(), 0u);
    EXPECT_EQ(sphere.indices.size() % 3, 0u);

    for (const auto& v : sphere.vertices) {
        // Vertex distance from center should equal radius
        EXPECT_NEAR(glm::length(v.position), 1.5f, 1e-3f);
        EXPECT_NEAR(glm::length(v.normal), 1.0f, 1e-4f);
    }
}

TEST(SyntheticGeneratorsTest, PartitionIntoMeshlets) {
    auto sphere = SyntheticMeshGenerator::GenerateSphere(1.0f, 20, 20);
    auto meshlets = SyntheticMeshGenerator::PartitionIntoMeshlets(sphere, 64, 124);

    EXPECT_GT(meshlets.size(), 0u);

    uint32_t totalTriangles = 0;
    for (const auto& m : meshlets) {
        EXPECT_LE(m.triangleCount, 124u);
        EXPECT_LE(m.vertexCount, 64u * 3u);
        EXPECT_GT(m.radius, 0.0f);
        EXPECT_NEAR(glm::length(m.coneAxis), 1.0f, 1e-4f);
        totalTriangles += m.triangleCount;
    }

    EXPECT_EQ(totalTriangles, sphere.indices.size() / 3);
}

// =============================================================================
// Synthetic Texture Generator Unit Tests
// =============================================================================

TEST(SyntheticGeneratorsTest, GenerateCheckerboardTexture) {
    auto tex = SyntheticTextureGenerator::GenerateCheckerboard(64, 64, 8,
        { 255, 0, 0, 255 }, { 0, 255, 0, 255 });

    EXPECT_EQ(tex.width, 64u);
    EXPECT_EQ(tex.height, 64u);
    EXPECT_EQ(tex.pixels.size(), 64u * 64u * 4u);

    // Tile (0, 0) should be ColorA (Red)
    auto p00 = tex.GetPixel(0, 0);
    EXPECT_EQ(p00.r, 255);
    EXPECT_EQ(p00.g, 0);
    EXPECT_EQ(p00.b, 0);
    EXPECT_EQ(p00.a, 255);

    // Tile (8, 0) should be ColorB (Green)
    auto p80 = tex.GetPixel(8, 0);
    EXPECT_EQ(p80.r, 0);
    EXPECT_EQ(p80.g, 255);
    EXPECT_EQ(p80.b, 0);
    EXPECT_EQ(p80.a, 255);
}

TEST(SyntheticGeneratorsTest, GenerateGradientTexture) {
    auto tex = SyntheticTextureGenerator::GenerateGradient(100, 10,
        { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f });

    EXPECT_EQ(tex.width, 100u);
    EXPECT_EQ(tex.height, 10u);
    EXPECT_EQ(tex.pixels.size(), 100u * 10u * 4u);

    auto pStart = tex.GetPixel(0, 0);
    EXPECT_NEAR(pStart.r, 1.0f, 1e-4f);
    EXPECT_NEAR(pStart.b, 0.0f, 1e-4f);

    auto pEnd = tex.GetPixel(99, 0);
    EXPECT_NEAR(pEnd.r, 0.0f, 1e-4f);
    EXPECT_NEAR(pEnd.b, 1.0f, 1e-4f);

    auto pMid = tex.GetPixel(50, 0);
    EXPECT_NEAR(pMid.r, 0.5f, 0.02f);
    EXPECT_NEAR(pMid.b, 0.5f, 0.02f);
}

TEST(SyntheticGeneratorsTest, GenerateSolidColorTexture) {
    auto tex = SyntheticTextureGenerator::GenerateSolidColor(32, 32, { 128, 64, 32, 255 });

    EXPECT_EQ(tex.width, 32u);
    EXPECT_EQ(tex.height, 32u);

    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 32; ++x) {
            auto p = tex.GetPixel(x, y);
            EXPECT_EQ(p.r, 128);
            EXPECT_EQ(p.g, 64);
            EXPECT_EQ(p.b, 32);
            EXPECT_EQ(p.a, 255);
        }
    }
}
