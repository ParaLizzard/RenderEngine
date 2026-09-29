#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Vulkan/VulkanRHI.h"
#include "AssetSystem/AssetManager.h"
#include "Threading/JobSubsystem.h"
#include "Core/SubsystemRegistry.h"

namespace Engine::Test {
    class VulkanRHITest : public ::testing::Test {};

    TEST_F(VulkanRHITest, LifecycleAndSubsystems) {
        SubsystemRegistry registry;
        registry.Register<VulkanRHI>();

        EXPECT_TRUE(registry.InitializeAll());
        EXPECT_TRUE(registry.Has<VulkanRHI>());

        auto& rhi = registry.Get<VulkanRHI>();
        EXPECT_EQ(rhi.GetName(), "VulkanRHI");
        EXPECT_NE(rhi.GetInstance(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetDevice().GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetDevice().GetPhysicalDevice(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetMemory().GetAllocator(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetBindlessHeap().GetDescriptorSet(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetPipelineCache().GetVkPipelineCache(), VK_NULL_HANDLE);
        EXPECT_NE(rhi.GetQueueManager().GetQueue(QueueType::Graphics), VK_NULL_HANDLE);
        EXPECT_EQ(rhi.GetSwapchain(), nullptr);

        registry.ShutdownAll();
    }

    TEST_F(VulkanRHITest, AssetManagerDependencyInjection) {
        SubsystemRegistry registry;
        registry.Register<VulkanRHI>();
        registry.Register<JobSubsystem>();
        registry.Register<AssetManager, JobSubsystem, VulkanRHI>();

        EXPECT_TRUE(registry.InitializeAll());

        auto& assetManager = registry.Get<AssetManager>();
        EXPECT_NE(assetManager.GetBindlessHeap(), nullptr);

        registry.ShutdownAll();
    }
}
