#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include "Common/VulkanTestContext.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Threading/JobSystem.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/GeometryBufferPool.h"
#include "Core/SubsystemRegistry.h"
#include "Threading/JobSubsystem.h"

namespace Engine::Test {

    class AssetManagerTest : public HeadlessVulkanTest {
    protected:
        void SetUp() override {
            HeadlessVulkanTest::SetUp();
        }
    };

    TEST_F(AssetManagerTest, LifecycleAndGeometryPoolAddresses) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);
        JobSystem jobSystem(2);

        AssetManager assetManager(device, memory, jobSystem);
        EXPECT_EQ(assetManager.GetName(), "AssetManager");

        GeometryBufferPool& pool = assetManager.GetGeometryPool();
        EXPECT_NE(pool.GetPositionBufferAddress(), 0u);
        EXPECT_NE(pool.GetAttributeBufferAddress(), 0u);
        EXPECT_NE(pool.GetIndexBufferAddress(), 0u);
        EXPECT_NE(pool.GetMeshletBufferAddress(), 0u);
        EXPECT_NE(pool.GetMeshletVerticesAddress(), 0u);
        EXPECT_NE(pool.GetMeshletTrianglesAddress(), 0u);

        EXPECT_EQ(pool.GetAllocatedVertices(), 0u);
        EXPECT_EQ(pool.GetAllocatedIndices(), 0u);
        EXPECT_EQ(pool.GetAllocatedMeshlets(), 0u);

        assetManager.Shutdown();
    }

    TEST_F(AssetManagerTest, HandleDeduplicationAndState) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);
        JobSystem jobSystem(2);

        AssetManager assetManager(device, memory, jobSystem);

        AssetHandle<Mesh> meshHandle1 = assetManager.LoadMeshAsync("models/sponza.glb");
        EXPECT_TRUE(meshHandle1.IsValid());
        EXPECT_EQ(meshHandle1.GetAssetType(), AssetType::Mesh);

        AssetHandle<Mesh> meshHandle2 = assetManager.LoadMeshAsync("models/sponza.glb");
        EXPECT_EQ(meshHandle1, meshHandle2);

        AssetHandle<VulkanTexture> texHandle1 = assetManager.LoadTextureAsync("textures/albedo.png", true);
        EXPECT_TRUE(texHandle1.IsValid());
        EXPECT_EQ(texHandle1.GetAssetType(), AssetType::Texture);

        AssetHandle<VulkanTexture> texHandle2 = assetManager.LoadTextureAsync("textures/albedo.png", true);
        EXPECT_EQ(texHandle1, texHandle2);

        EXPECT_NE(meshHandle1.GetID(), texHandle1.GetID());

        assetManager.Shutdown();
    }

    TEST_F(AssetManagerTest, GeometryPoolSubmeshAllocationAndUpload) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);
        JobSystem jobSystem(2);

        AssetManager assetManager(device, memory, jobSystem);
        GeometryBufferPool& pool = assetManager.GetGeometryPool();

        std::vector<VertexPositionGPU> positions = {
            { glm::vec3(-1.0f, -1.0f, 0.0f) },
            { glm::vec3( 1.0f, -1.0f, 0.0f) },
            { glm::vec3( 0.0f,  1.0f, 0.0f) }
        };

        std::vector<VertexAttributeGPU> attributes = {
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 }
        };

        std::vector<uint32_t> indices = { 0, 1, 2 };

        SubmeshGPUAllocation alloc = pool.AllocateSubmesh(
            static_cast<uint32_t>(positions.size()),
            static_cast<uint32_t>(indices.size()),
            0, 0, 0
        );

        EXPECT_EQ(alloc.vertexOffset, 0u);
        EXPECT_EQ(alloc.vertexCount, 3u);
        EXPECT_EQ(alloc.indexOffset, 0u);
        EXPECT_EQ(alloc.indexCount, 3u);

        EXPECT_NO_THROW({
            pool.UploadSubmesh(alloc, positions, attributes, indices, {}, {}, {});
        });

        EXPECT_EQ(pool.GetAllocatedVertices(), 3u);
        EXPECT_EQ(pool.GetAllocatedIndices(), 3u);

        assetManager.Shutdown();
    }

    TEST_F(AssetManagerTest, LoadMeshAsyncWithMaterialsAndBindlessHeap) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);
        JobSystem jobSystem(2);
        VulkanBindlessHeap bindlessHeap(device, 64);

        AssetManager assetManager(device, memory, jobSystem, &bindlessHeap);
        EXPECT_EQ(assetManager.GetBindlessHeap(), &bindlessHeap);

        std::string modelPath = "models/cube.glb";
        if (!std::filesystem::exists(modelPath)) {
            modelPath = "../../models/cube.glb";
        }

        if (std::filesystem::exists(modelPath)) {
            auto handle = assetManager.LoadMeshAsync(modelPath);
            assetManager.WaitAll();

            EXPECT_TRUE(assetManager.IsReady(handle));
            Mesh* mesh = assetManager.GetMesh(handle);
            ASSERT_NE(mesh, nullptr);
            EXPECT_GT(mesh->GetSubmeshCount(), 0u);
            EXPECT_GT(mesh->GetMaterials().size(), 0u);
            EXPECT_GT(mesh->GetGPUMaterials().size(), 0u);

            const auto* mat = mesh->GetMaterial(0);
            ASSERT_NE(mat, nullptr);
            const auto* gpuMat = mesh->GetGPUMaterial(0);
            ASSERT_NE(gpuMat, nullptr);
        }

        std::string spherePath = "models/pbr_sphere.glb";
        if (!std::filesystem::exists(spherePath)) {
            spherePath = "../../models/pbr_sphere.glb";
        }
        if (std::filesystem::exists(spherePath)) {
            auto sphereHandle = assetManager.LoadMeshAsync(spherePath);
            assetManager.WaitAll();

            EXPECT_TRUE(assetManager.IsReady(sphereHandle));
            Mesh* sphereMesh = assetManager.GetMesh(sphereHandle);
            ASSERT_NE(sphereMesh, nullptr);
            EXPECT_GT(sphereMesh->GetSubmeshCount(), 0u);
            EXPECT_GT(sphereMesh->GetMaterials().size(), 0u);
            EXPECT_GT(sphereMesh->GetGPUMaterials().size(), 0u);
        }

        assetManager.Shutdown();
    }

    TEST_F(AssetManagerTest, SubsystemRegistryIntegration) {
        ASSERT_NE(context->GetInstance(), VK_NULL_HANDLE);

        VulkanDeviceConfig config{};
        config.enableValidationLayers = true;
        config.requireDedicatedQueues = false;

        VulkanDevice device(context->GetInstance(), VK_NULL_HANDLE, config);
        VulkanMemory memory(device);

        SubsystemRegistry registry;
        registry.Register<JobSubsystem>();
        auto& assetManager = registry.Register<AssetManager, JobSubsystem>();

        EXPECT_EQ(assetManager.GetName(), "AssetManager");
        EXPECT_TRUE(registry.InitializeAll());

        assetManager.SetDeviceAndMemory(device, memory);

        std::string modelPath = "models/cube.glb";
        if (!std::filesystem::exists(modelPath)) {
            modelPath = "../../models/cube.glb";
        }
        if (std::filesystem::exists(modelPath)) {
            auto handle = assetManager.LoadMeshAsync(modelPath);
            assetManager.WaitAll();
            EXPECT_TRUE(assetManager.IsReady(handle));
        }

        registry.ShutdownAll();
    }

}
