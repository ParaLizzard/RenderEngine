#include <gtest/gtest.h>
#include "Renderer/RenderGraph.h"

// These tests exercise registerPhysicalImage/Buffer and the getter methods.
// The RenderGraph registry is just an unordered_map<string, GraphImage/Buffer>,
// so we can test by directly accessing via the public API.
// We need a Device& for the constructor but won't call any Vulkan functions
// in these specific paths. We use a trick: since RenderGraph only stores
// a reference to Device and doesn't call it during register/get operations,
// we cast a null to a Device& — tests only exercise the map logic.

namespace {
    // Create a reference to a null Device for testing map operations.
    // This is safe because register/get operations don't dereference device.
    Engine::Device &nullDevice()
    {
        static Engine::Device *ptr = nullptr;
        return *reinterpret_cast<Engine::Device *>(&ptr);
    }
} // namespace

class RenderGraphRegistryTest : public ::testing::Test {
protected:
    std::unique_ptr<Engine::RenderGraph> graph;

    void SetUp() override
    {
        graph = std::make_unique<Engine::RenderGraph>(nullDevice());
    }

    void TearDown() override
    {
        // clear() removes all internal state without Vulkan calls
        graph->clear();
        // Prevent destructor from calling Vulkan cleanup on transient cache
        graph.reset();
    }
};

// =============================================================================
// Image registration and retrieval
// =============================================================================

TEST_F(RenderGraphRegistryTest, RegisterAndGetImage)
{
    VkImage fakeImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xAAAA));
    VkImageView fakeView = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0xBBBB));

    graph->registerPhysicalImage("swapchain", fakeImage, fakeView,
                                  VK_FORMAT_B8G8R8A8_SRGB, {1920, 1080},
                                  VK_IMAGE_LAYOUT_UNDEFINED);

    EXPECT_EQ(graph->getImage("swapchain"), fakeImage);
    EXPECT_EQ(graph->getImageView("swapchain"), fakeView);
}

TEST_F(RenderGraphRegistryTest, RegisterMultipleImages)
{
    VkImage img1 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1111));
    VkImage img2 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x2222));
    VkImageView view1 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x3333));
    VkImageView view2 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x4444));

    graph->registerPhysicalImage("color", img1, view1, VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
    graph->registerPhysicalImage("depth", img2, view2, VK_FORMAT_D32_SFLOAT, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);

    EXPECT_EQ(graph->getImage("color"), img1);
    EXPECT_EQ(graph->getImage("depth"), img2);
    EXPECT_EQ(graph->getImageView("color"), view1);
    EXPECT_EQ(graph->getImageView("depth"), view2);
}

TEST_F(RenderGraphRegistryTest, ReRegisterImageOverwrites)
{
    VkImage img1 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1111));
    VkImage img2 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x2222));
    VkImageView view1 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x3333));
    VkImageView view2 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x4444));

    graph->registerPhysicalImage("target", img1, view1, VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);
    graph->registerPhysicalImage("target", img2, view2, VK_FORMAT_R8G8B8A8_UNORM, {800, 600}, VK_IMAGE_LAYOUT_UNDEFINED);

    EXPECT_EQ(graph->getImage("target"), img2);
    EXPECT_EQ(graph->getImageView("target"), view2);
}

// =============================================================================
// Unregistered image throws
// =============================================================================

TEST_F(RenderGraphRegistryTest, GetUnregisteredImageThrows)
{
    EXPECT_THROW(graph->getImage("nonexistent"), std::runtime_error);
}

TEST_F(RenderGraphRegistryTest, GetUnregisteredImageViewThrows)
{
    EXPECT_THROW(graph->getImageView("nonexistent"), std::runtime_error);
}

TEST_F(RenderGraphRegistryTest, GetUnregisteredImageThrowsContainsName)
{
    try {
        graph->getImage("myMissingResource");
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error &e) {
        EXPECT_NE(std::string(e.what()).find("myMissingResource"), std::string::npos)
            << "Error message should contain the resource name";
    }
}

// =============================================================================
// Buffer registration and retrieval
// =============================================================================

TEST_F(RenderGraphRegistryTest, RegisterAndGetBuffer)
{
    VkBuffer fakeBuf = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xDDDD));

    graph->registerPhysicalBuffer("objectSSBO", fakeBuf, 4096);

    auto info = graph->getBufferInfo("objectSSBO", 0);
    EXPECT_EQ(info.buffer, fakeBuf);
    EXPECT_EQ(info.offset, 0u);
    EXPECT_EQ(info.range, VK_WHOLE_SIZE);
}

TEST_F(RenderGraphRegistryTest, GetUnregisteredBufferThrows)
{
    EXPECT_THROW(graph->getBufferInfo("ghost", 0), std::runtime_error);
}

TEST_F(RenderGraphRegistryTest, GetUnregisteredBufferThrowsContainsName)
{
    try {
        graph->getBufferInfo("missingBuffer", 0);
        FAIL() << "Expected runtime_error";
    } catch (const std::runtime_error &e) {
        EXPECT_NE(std::string(e.what()).find("missingBuffer"), std::string::npos);
    }
}

// =============================================================================
// Buffer with custom initial stage/access
// =============================================================================

TEST_F(RenderGraphRegistryTest, RegisterBufferWithCustomInitialState)
{
    VkBuffer fakeBuf = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xEEEE));

    graph->registerPhysicalBuffer(
        "computeOut", fakeBuf, 2048,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT
    );

    // Should not throw — the buffer is registered
    auto info = graph->getBufferInfo("computeOut", 0);
    EXPECT_EQ(info.buffer, fakeBuf);
}
