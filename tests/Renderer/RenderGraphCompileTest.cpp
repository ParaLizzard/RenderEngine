#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Renderer/RenderGraph.h"

// Tests the compile() validation logic which checks that all declared
// image/buffer usages reference registered, non-null resources.

namespace {
    Engine::Device &nullDevice()
    {
        static Engine::Device *ptr = nullptr;
        return *reinterpret_cast<Engine::Device *>(&ptr);
    }

    // A minimal RenderPassNode that declares specific resource usages
    class StubPassNode : public Engine::RenderPassNode {
    public:
        std::vector<Engine::ImageUsageDeclaration> imagesToDeclare;
        std::vector<Engine::BufferUsageDeclaration> buffersToDeclare;

        void setup(Engine::RenderGraphBuilder &builder) override
        {
            for (auto &img : imagesToDeclare) {
                if (img.usageType == Engine::ResourceUsageType::Read) {
                    builder.readImage(img.imageName, img.imageLayout, img.stageMask, img.accessMask);
                } else {
                    builder.writeImage(img.imageName, img.imageLayout, img.stageMask, img.accessMask);
                }
            }
            for (auto &buf : buffersToDeclare) {
                builder.readBuffer(buf.bufferName, buf.stageMask, buf.accessMask);
            }
        }

        void execute(VkCommandBuffer &cmd, Engine::FrameInfo &frameInfo) override {}
    };
} // namespace

class RenderGraphCompileTest : public ::testing::Test {
protected:
    std::unique_ptr<Engine::RenderGraph> graph;

    void SetUp() override
    {
        graph = std::make_unique<Engine::RenderGraph>(nullDevice());
    }

    void TearDown() override
    {
        graph->clear();
        graph.reset();
    }
};

// =============================================================================
// Missing image → runtime_error
// =============================================================================

TEST_F(RenderGraphCompileTest, UnregisteredImageThrowsOnCompile)
{
    auto pass = std::make_unique<StubPassNode>();
    pass->imagesToDeclare.push_back({
        "missingImage",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    EXPECT_THROW(graph->compile(), std::runtime_error);
}

TEST_F(RenderGraphCompileTest, UnregisteredImageErrorContainsName)
{
    auto pass = std::make_unique<StubPassNode>();
    pass->imagesToDeclare.push_back({
        "gbufferAlbedo",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    try {
        graph->compile();
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error &e) {
        EXPECT_THAT(std::string(e.what()), ::testing::HasSubstr("gbufferAlbedo"));
    }
}

// =============================================================================
// Missing buffer → runtime_error
// =============================================================================

TEST_F(RenderGraphCompileTest, UnregisteredBufferThrowsOnCompile)
{
    auto pass = std::make_unique<StubPassNode>();
    pass->buffersToDeclare.push_back({
        "missingBuffer",
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    EXPECT_THROW(graph->compile(), std::runtime_error);
}

TEST_F(RenderGraphCompileTest, UnregisteredBufferErrorContainsName)
{
    auto pass = std::make_unique<StubPassNode>();
    pass->buffersToDeclare.push_back({
        "objectSSBO",
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    try {
        graph->compile();
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error &e) {
        EXPECT_THAT(std::string(e.what()), ::testing::HasSubstr("objectSSBO"));
    }
}

// =============================================================================
// Null handle image → runtime_error
// =============================================================================

TEST_F(RenderGraphCompileTest, NullImageHandleThrowsOnCompile)
{
    // Register an image with VK_NULL_HANDLE
    graph->registerPhysicalImage(
        "nullImage", VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED
    );

    auto pass = std::make_unique<StubPassNode>();
    pass->imagesToDeclare.push_back({
        "nullImage",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    EXPECT_THROW(graph->compile(), std::runtime_error);
}

// =============================================================================
// Null handle buffer → runtime_error
// =============================================================================

TEST_F(RenderGraphCompileTest, NullBufferHandleThrowsOnCompile)
{
    graph->registerPhysicalBuffer("nullBuffer", VK_NULL_HANDLE, 1024);

    auto pass = std::make_unique<StubPassNode>();
    pass->buffersToDeclare.push_back({
        "nullBuffer",
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    EXPECT_THROW(graph->compile(), std::runtime_error);
}

// =============================================================================
// Valid graph compiles without exception
// =============================================================================

TEST_F(RenderGraphCompileTest, ValidGraphCompilesSuccessfully)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xAAAA));
    VkImageView fakeView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0xBBBB));
    VkBuffer fakeBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xCCCC));

    graph->registerPhysicalImage("colorTarget", fakeImage, fakeView,
                                  VK_FORMAT_R8G8B8A8_UNORM, {1920, 1080},
                                  VK_IMAGE_LAYOUT_UNDEFINED);
    graph->registerPhysicalBuffer("objectData", fakeBuffer, 8192);

    auto pass = std::make_unique<StubPassNode>();
    pass->imagesToDeclare.push_back({
        "colorTarget",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        Engine::ResourceUsageType::Write
    });
    pass->buffersToDeclare.push_back({
        "objectData",
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass.get());

    EXPECT_NO_THROW(graph->compile());
}

// =============================================================================
// Empty graph compiles without exception
// =============================================================================

TEST_F(RenderGraphCompileTest, EmptyGraphCompiles)
{
    EXPECT_NO_THROW(graph->compile());
}

// =============================================================================
// Multiple passes with shared resources
// =============================================================================

TEST_F(RenderGraphCompileTest, MultiplePassesSharingResourcesCompile)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1111));
    VkImageView fakeView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x2222));

    graph->registerPhysicalImage("shared", fakeImage, fakeView,
                                  VK_FORMAT_R16G16B16A16_SFLOAT, {1920, 1080},
                                  VK_IMAGE_LAYOUT_UNDEFINED);

    auto pass1 = std::make_unique<StubPassNode>();
    pass1->imagesToDeclare.push_back({
        "shared",
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        Engine::ResourceUsageType::Write
    });

    auto pass2 = std::make_unique<StubPassNode>();
    pass2->imagesToDeclare.push_back({
        "shared",
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        Engine::ResourceUsageType::Read
    });

    graph->addPass(pass1.get());
    graph->addPass(pass2.get());

    EXPECT_NO_THROW(graph->compile());
}
