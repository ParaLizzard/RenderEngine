#include <gtest/gtest.h>
#include "AssetSystem/GLTFLoader.h"
#include "Threading/JobSystem.h"
#include <atomic>

namespace Engine::Test {

    TEST(FastGLTFParserTest, ParseGLTFFile) {
        auto result = GLTFLoader::LoadScene("models/cube.glb");

        ASSERT_TRUE(result.success);
        EXPECT_GE(result.meshes.size(), 1u);
        EXPECT_GE(result.GetTotalMeshCount(), 1u);

        const auto& mesh = result.meshes[0];
        ASSERT_FALSE(mesh.submeshes.empty());

        const auto& submesh = mesh.submeshes[0];
        EXPECT_FALSE(submesh.positions.empty());
        EXPECT_FALSE(submesh.indices.empty());
        EXPECT_FALSE(submesh.meshlets.empty());
        EXPECT_GT(submesh.boundingSphere.w, 0.0f);
        EXPECT_LT(submesh.aabbMin.x, submesh.aabbMax.x);
    }

    TEST(FastGLTFParserTest, AsyncBufferDecode) {
        JobSystem jobSystem(4);

        std::atomic<bool> completed = false;
        ParsedSceneResult loadedResult;

        auto handle = GLTFLoader::LoadSceneAsync(
            jobSystem,
            "models/pbr_sphere.glb",
            [&](ParsedSceneResult&& res) {
                loadedResult = std::move(res);
                completed = true;
            }
        );

        jobSystem.Wait(handle);

        EXPECT_TRUE(completed);
        ASSERT_TRUE(loadedResult.success);
        EXPECT_GT(loadedResult.GetTotalMeshletCount(), 0u);
        EXPECT_TRUE(loadedResult.HasHierarchy());
        EXPECT_FALSE(loadedResult.nodes.empty());
    }

    TEST(FastGLTFParserTest, InvalidFileHandling) {
        auto result = GLTFLoader::LoadScene("models/non_existent_file.glb");
        EXPECT_FALSE(result.success);
    }

}
