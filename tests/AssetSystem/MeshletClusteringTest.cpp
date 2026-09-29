#include <gtest/gtest.h>
#include "Common/SyntheticMeshGenerator.h"
#include "AssetSystem/GLTFLoader.h"
#include <meshoptimizer.h>

namespace Engine::Test {

    TEST(MeshletClusteringTest, SyntheticSphereMeshletConstraints) {
        SyntheticMesh sphere = SyntheticMeshGenerator::GenerateSphere(1.0f, 32, 32);
        ASSERT_FALSE(sphere.vertices.empty());
        ASSERT_FALSE(sphere.indices.empty());

        constexpr size_t maxVertices = 64;
        constexpr size_t maxTriangles = 124;
        constexpr float coneWeight = 0.5f;

        size_t maxMeshlets = meshopt_buildMeshletsBound(sphere.indices.size(), maxVertices, maxTriangles);
        std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
        std::vector<unsigned int> meshletVertices(maxMeshlets * maxVertices);
        std::vector<unsigned char> meshletTriangles(maxMeshlets * maxTriangles * 3);

        size_t meshletCount = meshopt_buildMeshlets(
            meshlets.data(),
            meshletVertices.data(),
            meshletTriangles.data(),
            sphere.indices.data(),
            sphere.indices.size(),
            &sphere.vertices[0].position.x,
            sphere.vertices.size(),
            sizeof(SyntheticVertex),
            maxVertices,
            maxTriangles,
            coneWeight
        );

        EXPECT_GT(meshletCount, 0u);
        meshlets.resize(meshletCount);

        for (const auto& m : meshlets) {
            EXPECT_LE(m.vertex_count, 64u);
            EXPECT_LE(m.triangle_count, 124u);
            EXPECT_GT(m.vertex_count, 0u);
            EXPECT_GT(m.triangle_count, 0u);

            meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                &meshletVertices[m.vertex_offset],
                &meshletTriangles[m.triangle_offset],
                m.triangle_count,
                &sphere.vertices[0].position.x,
                sphere.vertices.size(),
                sizeof(SyntheticVertex)
            );

            EXPECT_GT(bounds.radius, 0.0f);
            EXPECT_GE(bounds.cone_cutoff, -1.0f);
            EXPECT_LE(bounds.cone_cutoff, 1.0f);
        }
    }

    TEST(MeshletClusteringTest, PackConeAxisCutoffEncoding) {
        float axis[3] = { 0.0f, 1.0f, 0.0f };
        float cutoff = 0.75f;

        uint32_t packed = PackConeAxisCutoff(axis, cutoff);
        EXPECT_NE(packed, 0u);

        int8_t coneX = static_cast<int8_t>(packed & 0xFF);
        int8_t coneY = static_cast<int8_t>((packed >> 8) & 0xFF);
        int8_t coneZ = static_cast<int8_t>((packed >> 16) & 0xFF);
        int8_t coneCutoff = static_cast<int8_t>((packed >> 24) & 0xFF);

        EXPECT_EQ(coneX, 0);
        EXPECT_GT(coneY, 120);
        EXPECT_EQ(coneZ, 0);
        EXPECT_GT(coneCutoff, 90);
    }

    TEST(MeshletClusteringTest, GLTFModelMeshletValidation) {
        auto result = GLTFLoader::LoadScene("models/pbr_sphere.glb");
        ASSERT_TRUE(result.success);
        EXPECT_GT(result.GetTotalMeshletCount(), 0u);

        for (const auto& mesh : result.meshes) {
            for (const auto& submesh : mesh.submeshes) {
                for (const auto& m : submesh.meshlets) {
                    EXPECT_LE(m.vertexCount, 64u);
                    EXPECT_LE(m.triangleCount, 124u);
                    EXPECT_GT(m.vertexCount, 0u);
                    EXPECT_GT(m.triangleCount, 0u);
                    EXPECT_GT(m.radius, 0.0f);
                }
            }
        }
    }

    TEST(MeshletClusteringTest, SyntheticCubePartitioning) {
        SyntheticMesh cube = SyntheticMeshGenerator::GenerateCube();
        auto meshlets = SyntheticMeshGenerator::PartitionIntoMeshlets(cube, 64, 124);

        EXPECT_GT(meshlets.size(), 0u);
        for (const auto& m : meshlets) {
            EXPECT_LE(m.vertexCount, 64u);
            EXPECT_LE(m.triangleCount, 124u);
            EXPECT_GT(m.vertexCount, 0u);
            EXPECT_GT(m.triangleCount, 0u);
            EXPECT_GT(m.radius, 0.0f);
        }
    }

}
