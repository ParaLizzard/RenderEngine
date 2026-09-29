#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Common/SyntheticMeshGenerator.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "AssetSystem/GeometryBufferPool.h"

namespace Engine::Test {

    class GeometryBufferPoolTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    TEST_F(GeometryBufferPoolTest, SuballocationAndAddressResolution) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        GeometryBufferPool pool(device, memory, 10000, 30000, 1000);

        EXPECT_NE(pool.GetPositionBufferAddress(), 0u);
        EXPECT_NE(pool.GetAttributeBufferAddress(), 0u);
        EXPECT_NE(pool.GetIndexBufferAddress(), 0u);
        EXPECT_NE(pool.GetMeshletBufferAddress(), 0u);
        EXPECT_NE(pool.GetMeshletVerticesAddress(), 0u);
        EXPECT_NE(pool.GetMeshletTrianglesAddress(), 0u);

        auto alloc1 = pool.AllocateSubmesh(100, 300, 10, 640, 1200);
        auto alloc2 = pool.AllocateSubmesh(200, 600, 20, 1280, 2400);
        auto alloc3 = pool.AllocateSubmesh(50, 150, 5, 320, 600);

        EXPECT_EQ(alloc1.vertexOffset, 0u);
        EXPECT_EQ(alloc1.vertexCount, 100u);
        EXPECT_EQ(alloc1.indexOffset, 0u);
        EXPECT_EQ(alloc1.indexCount, 300u);
        EXPECT_EQ(alloc1.meshletOffset, 0u);
        EXPECT_EQ(alloc1.meshletCount, 10u);

        EXPECT_EQ(alloc2.vertexOffset, 100u);
        EXPECT_EQ(alloc2.vertexCount, 200u);
        EXPECT_EQ(alloc2.indexOffset, 300u);
        EXPECT_EQ(alloc2.indexCount, 600u);
        EXPECT_EQ(alloc2.meshletOffset, 10u);
        EXPECT_EQ(alloc2.meshletCount, 20u);

        EXPECT_EQ(alloc3.vertexOffset, 300u);
        EXPECT_EQ(alloc3.vertexCount, 50u);
        EXPECT_EQ(alloc3.indexOffset, 900u);
        EXPECT_EQ(alloc3.indexCount, 150u);
        EXPECT_EQ(alloc3.meshletOffset, 30u);
        EXPECT_EQ(alloc3.meshletCount, 5u);

        EXPECT_EQ(pool.GetAllocatedVertices(), 350u);
        EXPECT_EQ(pool.GetAllocatedIndices(), 1050u);
        EXPECT_EQ(pool.GetAllocatedMeshlets(), 35u);
    }

    TEST_F(GeometryBufferPoolTest, FreeAndSuballocation) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        GeometryBufferPool pool(device, memory, 1000, 3000, 100);

        auto alloc1 = pool.AllocateSubmesh(50, 150, 5, 320, 600);
        pool.FreeSubmesh(alloc1);

        auto alloc2 = pool.AllocateSubmesh(100, 300, 10, 640, 1200);
        EXPECT_EQ(alloc2.vertexCount, 100u);
        EXPECT_EQ(alloc2.indexCount, 300u);
        EXPECT_EQ(alloc2.meshletCount, 10u);
    }

    TEST_F(GeometryBufferPoolTest, DataUploadAndGPUIntegrity) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        GeometryBufferPool pool(device, memory, 1000, 3000, 100);

        std::vector<VertexPositionGPU> positions = {
            { { -1.0f, -1.0f, 0.0f } },
            { {  1.0f, -1.0f, 0.0f } },
            { {  1.0f,  1.0f, 0.0f } },
            { { -1.0f,  1.0f, 0.0f } }
        };
        std::vector<VertexAttributeGPU> attributes(4);
        for (auto& a : attributes) {
            a.tangentLo = 0;
            a.tangentHi = 0;
            a.uv = 0;
            a.normalOct = 0;
        }
        std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

        MeshletGPU meshlet{};
        meshlet.centerX = 0.0f;
        meshlet.centerY = 0.0f;
        meshlet.centerZ = 0.0f;
        meshlet.radius = 1.414f;
        meshlet.coneAxisCutoff = 0;
        meshlet.vertexOffset = 0;
        meshlet.triangleOffset = 0;
        meshlet.vertexCount = 4;
        meshlet.triangleCount = 2;

        std::vector<MeshletGPU> meshlets = { meshlet };
        std::vector<uint32_t> meshletVerts = { 0, 1, 2, 3 };
        std::vector<uint8_t> meshletTris = { 0, 1, 2, 2, 3, 0 };

        auto alloc = pool.AllocateSubmesh(
            static_cast<uint32_t>(positions.size()),
            static_cast<uint32_t>(indices.size()),
            static_cast<uint32_t>(meshlets.size()),
            static_cast<uint32_t>(meshletVerts.size()),
            static_cast<uint32_t>(meshletTris.size())
        );

        EXPECT_NO_THROW(pool.UploadSubmesh(
            alloc,
            positions,
            attributes,
            indices,
            meshlets,
            meshletVerts,
            meshletTris
        ));
    }

}
