#pragma once
#include "Core/ISubsystem.h"
#include "AssetSystem/AssetHandle.h"
#include "AssetSystem/GeometryBufferPool.h"
#include "AssetSystem/Mesh.h"
#include "AssetSystem/Material.h"
#include "Vulkan/VulkanTexture.h"
#include "Vulkan/VulkanBindlessHeap.h"
#include "Threading/JobSystem.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace Engine {
    class VulkanRHI;
    class AssetManager : public ISubsystem {
    public:
        AssetManager();
        AssetManager(VulkanDevice& device, VulkanMemory& memory, JobSystem& jobSystem, VulkanBindlessHeap* bindlessHeap = nullptr);
        ~AssetManager() override = default;

        std::string_view GetName() const override { return "AssetManager"; }
        bool Initialize(SubsystemRegistry& registry) override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        AssetHandle<Mesh> LoadMeshAsync(std::string_view filePath);
        AssetHandle<VulkanTexture> LoadTextureAsync(std::string_view filePath, bool srgb = true);

        Mesh* GetMesh(AssetHandle<Mesh> handle);
        VulkanTexture* GetTexture(AssetHandle<VulkanTexture> handle);
        GeometryBufferPool& GetGeometryPool() noexcept { return *geometryPool; }

        void SetDeviceAndMemory(VulkanDevice& inDevice, VulkanMemory& inMemory, VulkanBindlessHeap* inHeap = nullptr);
        void SetBindlessHeap(VulkanBindlessHeap* heap) noexcept { bindlessHeap = heap; }
        ENGINE_NODISCARD VulkanBindlessHeap* GetBindlessHeap() const noexcept { return bindlessHeap; }

        void WaitAll();

        AssetState GetAssetState(uint64_t handleId) const;

        template<typename T>
        bool IsReady(AssetHandle<T> handle) const {
            return GetAssetState(handle.GetID()) == AssetState::Ready;
        }

    private:
        VulkanDevice* device = nullptr;
        VulkanMemory* memory = nullptr;
        JobSystem* jobSystem = nullptr;
        VulkanBindlessHeap* bindlessHeap = nullptr;
        std::unique_ptr<GeometryBufferPool> geometryPool;

        mutable std::mutex assetMutex;
        mutable std::mutex jobMutex;
        std::vector<JobHandle> activeJobs;
        std::unordered_map<uint64_t, std::unique_ptr<Mesh>> meshes;
        std::unordered_map<uint64_t, std::unique_ptr<VulkanTexture>> textures;
        std::unordered_map<std::string, AssetHandle<Mesh>> pathToMesh;
        std::unordered_map<std::string, AssetHandle<VulkanTexture>> pathToTexture;
        std::unordered_map<uint64_t, AssetState> assetStates;
        uint32_t nextAssetIndex = 1;
    };
}