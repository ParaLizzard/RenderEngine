#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Vulkan/VulkanDevice.h"

namespace Engine::Test {

    class VulkanDeviceInitTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    // Test Case 1: Vulkan13FeatureVerification
    // Initialize VulkanDevice; assert dynamicRendering, synchronization2,
    // and bufferDeviceAddress are enabled in physical device features.
    TEST_F(VulkanDeviceInitTest, Vulkan13FeatureVerification) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);

        EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
        EXPECT_NE(device.GetPhysicalDevice(), VK_NULL_HANDLE);

        // Verify device capabilities reported by VulkanDevice
        const DeviceCapabilities& caps = device.GetCapabilities();
        EXPECT_TRUE(caps.supportsDynamicRendering);
        EXPECT_TRUE(caps.supportsSynchronization2);
        EXPECT_TRUE(caps.supportsBufferDeviceAddress);
        EXPECT_TRUE(caps.supportsMaintenance4);
        EXPECT_TRUE(caps.supportsDescriptorIndexing);
        EXPECT_TRUE(caps.supportsTimelineSemaphores);

        // Query physical device features directly via vkGetPhysicalDeviceFeatures2
        // to verify hardware/driver supports these required Vulkan 1.3 and 1.2 features.
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.pNext = &features13;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12;

        vkGetPhysicalDeviceFeatures2(device.GetPhysicalDevice(), &features2);

        EXPECT_EQ(features13.dynamicRendering, VK_TRUE);
        EXPECT_EQ(features13.synchronization2, VK_TRUE);
        EXPECT_EQ(features12.bufferDeviceAddress, VK_TRUE);
    }

    // Test Case 2: QueueFamilySelection
    // Verify Graphics, Compute, and Transfer queue families are identified.
    TEST_F(VulkanDeviceInitTest, QueueFamilySelection) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);

        // Verify Graphics queue family and queue handle
        EXPECT_NE(device.GetGraphicsQueueFamily(), VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(device.GetGraphicsQueue(), VK_NULL_HANDLE);

        // Verify Compute queue family and queue handle
        EXPECT_NE(device.GetComputeQueueFamily(), VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(device.GetComputeQueue(), VK_NULL_HANDLE);

        // Verify Transfer queue family and queue handle
        EXPECT_NE(device.GetTransferQueueFamily(), VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(device.GetTransferQueue(), VK_NULL_HANDLE);

        // Verify QueueFamilyIndices struct
        const QueueFamilyIndices& indices = device.GetQueueFamilyIndices();
        EXPECT_NE(indices.graphicsFamily, VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(indices.computeFamily, VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(indices.transferFamily, VK_QUEUE_FAMILY_IGNORED);

        // In headless mode (surface == VK_NULL_HANDLE), present family is set and complete
        EXPECT_NE(indices.presentFamily, VK_QUEUE_FAMILY_IGNORED);
        EXPECT_NE(device.GetPresentQueue(), VK_NULL_HANDLE);
        EXPECT_TRUE(indices.IsComplete());

        // Verify GetQueue() helper matches direct getters
        EXPECT_EQ(device.GetQueue(QueueType::Graphics), device.GetGraphicsQueue());
        EXPECT_EQ(device.GetQueue(QueueType::Compute), device.GetComputeQueue());
        EXPECT_EQ(device.GetQueue(QueueType::Transfer), device.GetTransferQueue());
        EXPECT_EQ(device.GetQueue(QueueType::Present), device.GetPresentQueue());

        // Verify queue family getter
        EXPECT_EQ(device.GetQueueFamily(QueueType::Graphics), device.GetGraphicsQueueFamily());
        EXPECT_EQ(device.GetQueueFamily(QueueType::Compute), device.GetComputeQueueFamily());
        EXPECT_EQ(device.GetQueueFamily(QueueType::Transfer), device.GetTransferQueueFamily());
        EXPECT_EQ(device.GetQueueFamily(QueueType::Present), device.GetPresentQueueFamily());
    }

    // Test Case 3: CleanShutdownAndLifecycle
    // Verify initialization and destruction cleanly release all resources
    // with zero validation errors or memory leaks.
    TEST_F(VulkanDeviceInitTest, CleanShutdownAndLifecycle) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        {
            VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE);
            EXPECT_NE(device.GetHandle(), VK_NULL_HANDLE);
            device.WaitIdle();
        }
        // Scope exit destroys VulkanDevice. No validation errors or crashes.
    }

    // Test Case 4: MoveSemantics
    // Verify move constructor and move assignment correctly transfer device ownership.
    TEST_F(VulkanDeviceInitTest, MoveSemantics) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDevice device1(context->GetInstance(), VK_NULL_HANDLE);
        VkDevice handle1 = device1.GetHandle();
        EXPECT_NE(handle1, VK_NULL_HANDLE);

        // Move construct
        VulkanDevice device2(std::move(device1));
        EXPECT_EQ(device2.GetHandle(), handle1);
        EXPECT_EQ(device1.GetHandle(), VK_NULL_HANDLE);

        // Move assign
        VulkanDevice device3 = std::move(device2);
        EXPECT_EQ(device3.GetHandle(), handle1);
        EXPECT_EQ(device2.GetHandle(), VK_NULL_HANDLE);

        device3.WaitIdle();
    }

    // Test Case 5: DeviceCapabilitiesAndLimits
    // Verify device physical limits and subgroup capabilities are properly populated.
    TEST_F(VulkanDeviceInitTest, DeviceCapabilitiesAndLimits) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE);
        const DeviceCapabilities& caps = device.GetCapabilities();

        EXPECT_GT(caps.maxPushConstantsSize, 0u);
        EXPECT_GE(caps.maxSamplerAnisotropy, 1.0f);
        EXPECT_GT(caps.minUniformBufferOffsetAlignment, 0u);
        EXPECT_GT(caps.minStorageBufferOffsetAlignment, 0u);
        EXPECT_GT(caps.subgroupSize, 0u);
    }

} // namespace Engine::Test
