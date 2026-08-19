#include <gtest/gtest.h>
#include "Common/VulkanTestContext.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <fstream>

using namespace Engine::Test;

class BaselineVerificationFixture : public HeadlessVulkanTest {
protected:
    std::filesystem::path FindWorkspaceRoot() const {
        std::filesystem::path current = std::filesystem::current_path();
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(current / "CMakeLists.txt") && std::filesystem::exists(current / "docs") && std::filesystem::exists(current / "models")) {
                return current;
            }
            if (current.has_parent_path()) {
                current = current.parent_path();
            }
        }
        return std::filesystem::current_path();
    }
};

TEST_F(BaselineVerificationFixture, BaselineDirectoriesAndAssetsExist) {
    auto root = FindWorkspaceRoot();

    EXPECT_TRUE(std::filesystem::exists(root / "models")) << "models/ directory must exist in workspace";
    EXPECT_TRUE(std::filesystem::exists(root / "models" / "cube.glb")) << "models/cube.glb must exist";
    EXPECT_TRUE(std::filesystem::exists(root / "shaders")) << "shaders/ directory must exist in workspace";
}

TEST_F(BaselineVerificationFixture, FastGLTFCubeParsing) {
    auto root = FindWorkspaceRoot();
    auto cubePath = root / "models" / "cube.glb";
    ASSERT_TRUE(std::filesystem::exists(cubePath));

    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(cubePath);
    ASSERT_TRUE(bool(data)) << "Failed to read cube.glb into memory";

    auto asset = parser.loadGltfBinary(data.get(), cubePath.parent_path(), fastgltf::Options::None);
    ASSERT_EQ(asset.error(), fastgltf::Error::None) << "FastGLTF failed to parse cube.glb";

    EXPECT_GT(asset->meshes.size(), 0u);
    EXPECT_GT(asset->nodes.size(), 0u);
}

TEST_F(BaselineVerificationFixture, Vulkan13HardwareCapabilityBaseline) {
    ASSERT_TRUE(context != nullptr);
    ASSERT_TRUE(context->IsInitialized());

    // Verify Vulkan 1.3 core and required extensions for RenderEngine
    EXPECT_TRUE(context->SupportsSynchronization2()) << "GPU must support Synchronization2 (Vulkan 1.3)";
    EXPECT_TRUE(context->SupportsDynamicRendering()) << "GPU must support Dynamic Rendering (Vulkan 1.3)";

    const auto& features2 = context->GetFeatures();
    EXPECT_EQ(features2.features.samplerAnisotropy, VK_TRUE) << "GPU must support Sampler Anisotropy";
    EXPECT_EQ(features2.features.depthClamp, VK_TRUE) << "GPU must support Depth Clamping";
}

TEST_F(BaselineVerificationFixture, GoldenBaselinesDocumentationPresent) {
    auto root = FindWorkspaceRoot();

    EXPECT_TRUE(std::filesystem::exists(root / "docs" / "baselines" / "baseline_perf.csv"))
        << "docs/baselines/baseline_perf.csv must be present";
    EXPECT_TRUE(std::filesystem::exists(root / "docs" / "baselines" / "README.md"))
        << "docs/baselines/README.md must be present";
    EXPECT_TRUE(std::filesystem::exists(root / "docs" / "nvidia_trace_analysis.txt"))
        << "docs/nvidia_trace_analysis.txt must be present";
}
