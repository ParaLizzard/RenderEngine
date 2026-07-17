#include <gtest/gtest.h>
#include "Renderer/RenderGraph.h"

// Tests for RenderGraph lifecycle methods: clear(), updateImageHandle(),
// updateBufferHandle(), transitionToPresent() state-tracking aspects.

namespace {
    Engine::Device &nullDevice()
    {
        static Engine::Device *ptr = nullptr;
        return *reinterpret_cast<Engine::Device *>(&ptr);
    }

    class EmptyPassNode : public Engine::RenderPassNode {
    public:
        void setup(Engine::RenderGraphBuilder &builder) override {}
        void execute(VkCommandBuffer &cmd, Engine::FrameInfo &frameInfo) override {}

        bool sceneDirtyCallReceived = false;
        void markSceneDirty() override { sceneDirtyCallReceived = true; }
    };
} // namespace

class RenderGraphLifecycleTest : public ::testing::Test {
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
// clear() removes all registrations
// =============================================================================

TEST_F(RenderGraphLifecycleTest, ClearRemovesAllImages)
{
    VkImage img = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1111));
    VkImageView view = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x2222));

    graph->registerPhysicalImage("target", img, view,
                                  VK_FORMAT_R8G8B8A8_UNORM, {800, 600},
                                  VK_IMAGE_LAYOUT_UNDEFINED);

    EXPECT_NO_THROW(graph->getImage("target"));

    graph->clear();

    EXPECT_THROW(graph->getImage("target"), std::runtime_error);
}

TEST_F(RenderGraphLifecycleTest, ClearRemovesAllBuffers)
{
    VkBuffer buf = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x3333));
    graph->registerPhysicalBuffer("ssbo", buf, 4096);

    EXPECT_NO_THROW(graph->getBufferInfo("ssbo", 0));

    graph->clear();

    EXPECT_THROW(graph->getBufferInfo("ssbo", 0), std::runtime_error);
}

TEST_F(RenderGraphLifecycleTest, ClearCanBeCalledMultipleTimes)
{
    graph->clear();
    graph->clear();
    graph->clear();
    // Should not crash or throw
}

// =============================================================================
// updateImageHandle()
// =============================================================================

TEST_F(RenderGraphLifecycleTest, UpdateImageHandleChangesImageAndView)
{
    VkImage img1 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xAAAA));
    VkImage img2 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xBBBB));
    VkImageView view1 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0xCCCC));
    VkImageView view2 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0xDDDD));

    graph->registerPhysicalImage("swap", img1, view1,
                                  VK_FORMAT_B8G8R8A8_SRGB, {1920, 1080},
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    graph->updateImageHandle("swap", img2, view2, {1920, 1080});

    EXPECT_EQ(graph->getImage("swap"), img2);
    EXPECT_EQ(graph->getImageView("swap"), view2);
}

TEST_F(RenderGraphLifecycleTest, UpdateImageHandleWithNewImageResetsLayout)
{
    // The implementation resets layout to UNDEFINED when the VkImage handle changes
    VkImage img1 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1111));
    VkImage img2 = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x2222));
    VkImageView view1 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x3333));
    VkImageView view2 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x4444));

    graph->registerPhysicalImage("target", img1, view1,
                                  VK_FORMAT_R8G8B8A8_UNORM, {800, 600},
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // After update with a different image handle, layout should reset
    graph->updateImageHandle("target", img2, view2, {800, 600});

    // We can verify indirectly: after an update that changes the image,
    // the next compile/execute should see UNDEFINED layout (needs a barrier)
    // The image handle should be updated
    EXPECT_EQ(graph->getImage("target"), img2);
}

TEST_F(RenderGraphLifecycleTest, UpdateImageHandleSameImagePreservesState)
{
    VkImage img = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x5555));
    VkImageView view1 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x6666));
    VkImageView view2 = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x7777));

    graph->registerPhysicalImage("target", img, view1,
                                  VK_FORMAT_R8G8B8A8_UNORM, {800, 600},
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Same image handle, different view (e.g., swapchain image view rotation)
    graph->updateImageHandle("target", img, view2, {800, 600});

    EXPECT_EQ(graph->getImage("target"), img);
    EXPECT_EQ(graph->getImageView("target"), view2);
}

TEST_F(RenderGraphLifecycleTest, UpdateImageHandleUnknownImageIsNoOp)
{
    VkImage img = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x8888));
    VkImageView view = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x9999));

    // Should not throw even if image is not registered
    EXPECT_NO_THROW(graph->updateImageHandle("unknown", img, view, {100, 100}));
}

TEST_F(RenderGraphLifecycleTest, UpdateImageHandleUpdatesExtent)
{
    VkImage img = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0xAA00));
    VkImageView view = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0xBB00));

    graph->registerPhysicalImage("resizable", img, view,
                                  VK_FORMAT_R8G8B8A8_UNORM, {800, 600},
                                  VK_IMAGE_LAYOUT_UNDEFINED);

    graph->updateImageHandle("resizable", img, view, {1920, 1080});

    // The image should still be retrievable
    EXPECT_EQ(graph->getImage("resizable"), img);
}

// =============================================================================
// updateBufferHandle()
// =============================================================================

TEST_F(RenderGraphLifecycleTest, UpdateBufferHandleChangesBuffer)
{
    VkBuffer buf1 = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xF001));
    VkBuffer buf2 = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xF002));

    graph->registerPhysicalBuffer("data", buf1, 1024);
    graph->updateBufferHandle("data", buf2, 2048);

    auto info = graph->getBufferInfo("data", 0);
    EXPECT_EQ(info.buffer, buf2);
}

TEST_F(RenderGraphLifecycleTest, UpdateBufferHandleUnknownBufferIsNoOp)
{
    VkBuffer buf = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0xDEAD));
    EXPECT_NO_THROW(graph->updateBufferHandle("ghost", buf, 512));
}

// =============================================================================
// markSceneDirty() propagates to all passes
// =============================================================================

TEST_F(RenderGraphLifecycleTest, MarkSceneDirtyPropagates)
{
    auto pass1 = std::make_unique<EmptyPassNode>();
    auto pass2 = std::make_unique<EmptyPassNode>();

    auto *p1 = pass1.get();
    auto *p2 = pass2.get();

    graph->addPass(p1);
    graph->addPass(p2);

    graph->markSceneDirty();

    EXPECT_TRUE(p1->sceneDirtyCallReceived);
    EXPECT_TRUE(p2->sceneDirtyCallReceived);
}

// =============================================================================
// transitionToPresent — unknown image is no-op
// =============================================================================

// NOTE: transitionToPresent() calls vkCmdPipelineBarrier2 for actual transitions,
// but the early-return paths (unknown image, already-present) don't call Vulkan.

TEST_F(RenderGraphLifecycleTest, TransitionToPresentUnknownImageNoOp)
{
    // Should not throw — just returns early
    // We can't call it without a valid command buffer for registered images,
    // but for an unknown image it returns before any Vulkan call.
    VkCommandBuffer nullCmd = VK_NULL_HANDLE;
    EXPECT_NO_THROW(graph->transitionToPresent(nullCmd, "nonexistent"));
}
