#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"

namespace Engine::Test {

    class VulkanMemoryBudgetTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    // Test Case 1: AllocAndFreeMemory
    // Allocate 64MB GPU buffer via VulkanMemory; verify allocation success;
    // free memory; verify heap usage decreases.
    TEST_F(VulkanMemoryBudgetTest, AllocAndFreeMemory) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        ASSERT_NE(memory.GetAllocator(), VK_NULL_HANDLE);

        // Record initial memory budget
        const VRAMBudget initialBudget = memory.GetVRAMBudget();

        // Configure 64MB buffer
        constexpr VkDeviceSize bufferSize = 64 * 1024 * 1024; // 64 MB
        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        if (device.GetCapabilities().supportsBufferDeviceAddress) {
            bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }

        VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo resultAllocInfo{};

        VkResult res = vmaCreateBuffer(
            memory.GetAllocator(),
            &bufferInfo,
            &allocInfo,
            &buffer,
            &allocation,
            &resultAllocInfo
        );

        ASSERT_EQ(res, VK_SUCCESS);
        EXPECT_NE(buffer, VK_NULL_HANDLE);
        EXPECT_NE(allocation, VK_NULL_HANDLE);
        EXPECT_GE(resultAllocInfo.size, bufferSize);

        // Check Buffer Device Address alignment if BDA is supported (min 16-byte alignment)
        if (device.GetCapabilities().supportsBufferDeviceAddress) {
            EXPECT_EQ(resultAllocInfo.offset % 16, 0u);
        }

        // Verify heap usage after allocation
        const VRAMBudget allocatedBudget = memory.GetVRAMBudget();
        EXPECT_GE(allocatedBudget.totalHeapUsage, initialBudget.totalHeapUsage);

        // Free memory
        vmaDestroyBuffer(memory.GetAllocator(), buffer, allocation);

        // Verify heap usage decreases back
        const VRAMBudget freedBudget = memory.GetVRAMBudget();
        EXPECT_LE(freedBudget.totalHeapUsage, allocatedBudget.totalHeapUsage);
    }

    // Test Case 2: MemoryBudgetQuery
    // Call VulkanMemory::GetBudget(); assert valid heap usage and total budget.
    TEST_F(VulkanMemoryBudgetTest, MemoryBudgetQuery) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        ASSERT_NE(memory.GetAllocator(), VK_NULL_HANDLE);

        // Query memory budget via GetVRAMBudget()
        const VRAMBudget vramBudget = memory.GetVRAMBudget();

        // Total heap budget for physical device should be strictly positive (e.g. > 0 MB)
        EXPECT_GT(vramBudget.totalHeapBudget, 0u);

        // Available memory should be less than or equal to total heap budget
        EXPECT_LE(vramBudget.availableMemory, vramBudget.totalHeapBudget);

        // Total heap usage should be less than or equal to total heap budget
        EXPECT_LE(vramBudget.totalHeapUsage, vramBudget.totalHeapBudget);
    }

    // Test Case 3: CleanShutdownAndZeroLeaks
    // Verify zero memory leaks or validation warnings when allocator shuts down.
    TEST_F(VulkanMemoryBudgetTest, CleanShutdownAndZeroLeaks) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE);

        {
            VulkanMemory memory(device);
            EXPECT_NE(memory.GetAllocator(), VK_NULL_HANDLE);

            // Allocate a smaller test buffer
            VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = 1024 * 1024, // 1 MB
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
            };
            VmaAllocationCreateInfo allocInfo{
                .usage = VMA_MEMORY_USAGE_AUTO
            };

            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            ASSERT_EQ(vmaCreateBuffer(memory.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr), VK_SUCCESS);

            // Free the buffer cleanly
            vmaDestroyBuffer(memory.GetAllocator(), buffer, allocation);
        }
        // Scope exit destroys VulkanMemory -> vmaDestroyAllocator.
        // If anything was leaked, VMA assertions or validation layer will trigger.
    }

    // Test Case 4: MoveSemantics
    // Verify move constructor and move assignment properly transfer allocator ownership.
    TEST_F(VulkanMemoryBudgetTest, MoveSemantics) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE);

        VulkanMemory mem1(device);
        VmaAllocator rawAlloc = mem1.GetAllocator();
        EXPECT_NE(rawAlloc, VK_NULL_HANDLE);

        // Move construct
        VulkanMemory mem2(std::move(mem1));
        EXPECT_EQ(mem2.GetAllocator(), rawAlloc);
        EXPECT_EQ(mem1.GetAllocator(), VK_NULL_HANDLE);

        // Move assign
        VulkanMemory mem3(device);
        mem3 = std::move(mem2);
        EXPECT_EQ(mem3.GetAllocator(), rawAlloc);
        EXPECT_EQ(mem2.GetAllocator(), VK_NULL_HANDLE);
    }

    // Test Case 5: HostVisibleUploadAndMap
    // Verify host visible memory mapping and data writeback integrity.
    TEST_F(VulkanMemoryBudgetTest, HostVisibleUploadAndMap) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE);
        VulkanMemory memory(device);

        constexpr size_t elementCount = 1024;
        constexpr VkDeviceSize bufferSize = elementCount * sizeof(uint32_t);

        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };

        VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocResult{};

        ASSERT_EQ(vmaCreateBuffer(memory.GetAllocator(), &bufferInfo, &allocInfo, &buffer, &allocation, &allocResult), VK_SUCCESS);
        ASSERT_NE(allocResult.pMappedData, nullptr);

        // Write test pattern
        uint32_t* mappedArray = static_cast<uint32_t*>(allocResult.pMappedData);
        for (size_t i = 0; i < elementCount; ++i) {
            mappedArray[i] = static_cast<uint32_t>(i * 42);
        }

        // Flush if needed
        vmaFlushAllocation(memory.GetAllocator(), allocation, 0, bufferSize);

        // Verify written data
        for (size_t i = 0; i < elementCount; ++i) {
            EXPECT_EQ(mappedArray[i], static_cast<uint32_t>(i * 42));
        }

        vmaDestroyBuffer(memory.GetAllocator(), buffer, allocation);
    }

} // namespace Engine::Test
