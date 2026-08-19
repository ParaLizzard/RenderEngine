#include <gtest/gtest.h>
#include "Common/VulkanTestContext.h"
#include "Common/TestUtils.h"

using namespace Engine::Test;

TEST_F(HeadlessVulkanTest, HeadlessVulkanInitializesSuccessfully) {
    ASSERT_TRUE(context != nullptr);
    EXPECT_TRUE(context->IsInitialized());
    EXPECT_NE(context->GetInstance(), VK_NULL_HANDLE);
    EXPECT_NE(context->GetPhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_NE(context->GetDevice(), VK_NULL_HANDLE);
    EXPECT_NE(context->GetGraphicsQueue(), VK_NULL_HANDLE);
    EXPECT_NE(context->GetAllocator(), VK_NULL_HANDLE);
}

TEST_F(HeadlessVulkanTest, QueueFamiliesAreValid) {
    EXPECT_NE(context->GetGraphicsQueueFamily(), ~0u);
    EXPECT_NE(context->GetComputeQueueFamily(), ~0u);
}

TEST_F(HeadlessVulkanTest, Vulkan13CoreFeaturesSupported) {
    // RenderEngine requires Vulkan 1.3 features
    EXPECT_TRUE(context->SupportsSynchronization2());
    EXPECT_TRUE(context->SupportsDynamicRendering());
}

TEST_F(HeadlessVulkanTest, AllocateAndFreeGpuMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1024 * 1024; // 1 MB
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo resultAllocInfo{};

    VkResult res = vmaCreateBuffer(context->GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &resultAllocInfo);
    ASSERT_EQ(res, VK_SUCCESS);
    ASSERT_NE(buffer, VK_NULL_HANDLE);
    ASSERT_NE(allocation, VK_NULL_HANDLE);
    ASSERT_NE(resultAllocInfo.pMappedData, nullptr);

    // Write a test pattern to mapped memory
    uint32_t* mappedData = static_cast<uint32_t*>(resultAllocInfo.pMappedData);
    for (size_t i = 0; i < 256; ++i) {
        mappedData[i] = static_cast<uint32_t>(i * 0x12345678);
    }

    // Read back and verify pattern
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_EQ(mappedData[i], static_cast<uint32_t>(i * 0x12345678));
    }

    vmaDestroyBuffer(context->GetAllocator(), buffer, allocation);
}
