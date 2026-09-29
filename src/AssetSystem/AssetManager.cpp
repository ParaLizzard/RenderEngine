#include "AssetManager.h"
#include "GLTFLoader.h"
#include "TextureLoader.h"
#include "Threading/JobSubsystem.h"
#include "Vulkan/VulkanRHI.h"
#include "Core/SubsystemRegistry.h"
#include "Core/Assert.h"
#include <filesystem>

namespace Engine
{
    AssetManager::AssetManager() = default;

    AssetManager::AssetManager(VulkanDevice &device, VulkanMemory &memory, JobSystem &jobSystem, VulkanBindlessHeap* bindlessHeap)
        : device(&device), memory(&memory), jobSystem(&jobSystem), bindlessHeap(bindlessHeap)
    {
        geometryPool = std::make_unique<GeometryBufferPool>(device, memory);
    }

    void AssetManager::SetDeviceAndMemory(VulkanDevice& inDevice, VulkanMemory& inMemory, VulkanBindlessHeap* inHeap)
    {
        device = &inDevice;
        memory = &inMemory;
        if (inHeap) {
            bindlessHeap = inHeap;
        }
        if (!geometryPool) {
            geometryPool = std::make_unique<GeometryBufferPool>(*device, *memory);
        }
    }

    bool AssetManager::Initialize(SubsystemRegistry &registry)
    {
        if (auto* rhi = registry.TryGet<VulkanRHI>()) {
            device = &rhi->GetDevice();
            memory = &rhi->GetMemory();
            bindlessHeap = &rhi->GetBindlessHeap();
        }

        if (!jobSystem) {
            if (auto* js = registry.TryGet<JobSubsystem>()) {
                jobSystem = &js->GetJobSystem();
            }
        }

        if (device && memory && !geometryPool) {
            geometryPool = std::make_unique<GeometryBufferPool>(*device, *memory);
        }
        return true;
    }

    void AssetManager::Update(float deltaTime)
    {
        ISubsystem::Update(deltaTime);
    }

    void AssetManager::Shutdown()
    {
        WaitAll();

        std::lock_guard<std::mutex> lock(assetMutex);
        meshes.clear();
        textures.clear();
        pathToMesh.clear();
        pathToTexture.clear();
        assetStates.clear();
        geometryPool.reset();
    }

    void AssetManager::WaitAll()
    {
        std::vector<JobHandle> jobsToWait;
        {
            std::lock_guard<std::mutex> lock(jobMutex);
            jobsToWait = std::move(activeJobs);
            activeJobs.clear();
        }
        if (jobSystem) {
            for (const auto& job : jobsToWait) {
                jobSystem->Wait(job);
            }
        }
    }

    AssetHandle<Mesh> AssetManager::LoadMeshAsync(std::string_view filePath)
    {
        std::string pathStr(filePath);
        {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = pathToMesh.find(pathStr);
            if (it != pathToMesh.end()) {
                return it->second;
            }
        }

        uint32_t index = nextAssetIndex++;
        AssetHandle<Mesh> handle = AssetHandle<Mesh>::Create(index, 1);

        {
            std::lock_guard<std::mutex> lock(assetMutex);
            pathToMesh[pathStr] = handle;
            assetStates[handle.GetID()] = AssetState::Loading;
        }

        ENGINE_ASSERT(jobSystem, "AssetManager: JobSystem is not initialized");
        JobHandle job = GLTFLoader::LoadSceneAsync(*jobSystem, filePath, [this, handle, pathStr](ParsedSceneResult&& result) {
            if (!result.success || result.meshes.empty()) {
                std::lock_guard<std::mutex> lock(assetMutex);
                assetStates[handle.GetID()] = AssetState::Failed;
                return;
            }

            std::vector<AssetHandle<VulkanTexture>> loadedTextures;
            loadedTextures.resize(result.textures.size());

            struct TextureDecodeTask {
                size_t textureIndex = 0;
                AssetHandle<VulkanTexture> handle;
                DecodedImageData decoded;
            };

            std::vector<TextureDecodeTask> decodeTasks;
            decodeTasks.reserve(result.textures.size());

            for (size_t i = 0; i < result.textures.size(); ++i) {
                const auto& texSource = result.textures[i];
                std::string texKey;
                if (!texSource.uri.empty()) {
                    texKey = texSource.uri;
                } else {
                    texKey = pathStr + "#tex_" + (texSource.name.empty() ? std::to_string(i) : texSource.name);
                }

                AssetHandle<VulkanTexture> texHandle;
                bool needLoad = false;
                {
                    std::lock_guard<std::mutex> lock(assetMutex);
                    auto it = pathToTexture.find(texKey);
                    if (it != pathToTexture.end()) {
                        texHandle = it->second;
                    } else {
                        uint32_t texIdx = nextAssetIndex++;
                        texHandle = AssetHandle<VulkanTexture>::Create(texIdx, 1);
                        pathToTexture[texKey] = texHandle;
                        assetStates[texHandle.GetID()] = AssetState::Loading;
                        needLoad = true;
                    }
                }

                loadedTextures[i] = texHandle;

                if (needLoad) {
                    decodeTasks.push_back(TextureDecodeTask{
                        .textureIndex = i,
                        .handle = texHandle
                    });
                }
            }

            std::optional<JobHandle> decodeJob;
            if (decodeTasks.size() > 1 && jobSystem) {
                decodeJob = jobSystem->Dispatch(
                    static_cast<uint32_t>(decodeTasks.size()),
                    1,
                    [&result, &decodeTasks](uint32_t start, uint32_t end) {
                        for (uint32_t idx = start; idx < end; ++idx) {
                            auto& task = decodeTasks[idx];
                            const auto& texSource = result.textures[task.textureIndex];
                            if (!texSource.embeddedData.empty()) {
                                std::string ext = texSource.isKTX2 ? ".ktx2" : (texSource.mimeType.find("jpeg") != std::string::npos ? ".jpg" : ".png");
                                task.decoded = TextureLoader::LoadFromMemory(texSource.embeddedData, ext, texSource.isSRGB);
                            } else if (!texSource.uri.empty()) {
                                task.decoded = TextureLoader::LoadFromFile(texSource.uri, texSource.isSRGB);
                            }
                        }
                    }
                );
            } else if (decodeTasks.size() == 1) {
                auto& task = decodeTasks[0];
                const auto& texSource = result.textures[task.textureIndex];
                if (!texSource.embeddedData.empty()) {
                    std::string ext = texSource.isKTX2 ? ".ktx2" : (texSource.mimeType.find("jpeg") != std::string::npos ? ".jpg" : ".png");
                    task.decoded = TextureLoader::LoadFromMemory(texSource.embeddedData, ext, texSource.isSRGB);
                } else if (!texSource.uri.empty()) {
                    task.decoded = TextureLoader::LoadFromFile(texSource.uri, texSource.isSRGB);
                }
            }

            std::vector<Submesh> submeshes;
            for (auto& meshData : result.meshes) {
                for (auto& subData : meshData.submeshes) {
                    auto alloc = geometryPool->AllocateSubmesh(
                        static_cast<uint32_t>(subData.positions.size()),
                        static_cast<uint32_t>(subData.indices.size()),
                        static_cast<uint32_t>(subData.meshlets.size()),
                        static_cast<uint32_t>(subData.meshletVertices.size()),
                        static_cast<uint32_t>(subData.meshletTriangles.size())
                    );

                    geometryPool->UploadSubmesh(
                        alloc,
                        subData.positions,
                        subData.attributes,
                        subData.indices,
                        subData.meshlets,
                        subData.meshletVertices,
                        subData.meshletTriangles
                    );

                    submeshes.push_back(Submesh{
                        .name = std::move(subData.name),
                        .gpuAllocation = alloc,
                        .materialIndex = subData.materialIndex,
                        .boundingSphere = subData.boundingSphere,
                        .aabbMin = subData.aabbMin,
                        .aabbMax = subData.aabbMax
                    });
                }
            }

            if (decodeJob.has_value() && jobSystem) {
                jobSystem->Wait(*decodeJob);
            }

            for (auto& task : decodeTasks) {
                const auto& texSource = result.textures[task.textureIndex];
                std::unique_ptr<VulkanTexture> gpuTexture;
                if (!task.decoded.pixelData.empty() && device && memory) {
                    gpuTexture = TextureLoader::UploadToGPU(*device, *memory, task.decoded, true, texSource.name);
                }

                if (gpuTexture) {
                    if (bindlessHeap) {
                        auto bindlessHandle = bindlessHeap->RegisterTexture(gpuTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
                        gpuTexture->SetBindlessSlot(bindlessHandle.slot);
                    }

                    std::lock_guard<std::mutex> lock(assetMutex);
                    textures[task.handle.GetID()] = std::move(gpuTexture);
                    assetStates[task.handle.GetID()] = AssetState::Ready;
                } else {
                    std::lock_guard<std::mutex> lock(assetMutex);
                    assetStates[task.handle.GetID()] = AssetState::Failed;
                }
            }

            std::vector<MaterialGPU> gpuMaterials;
            gpuMaterials.reserve(result.materials.size());

            auto getBindlessSlot = [&](int32_t texIdx) -> uint32_t {
                if (texIdx < 0 || static_cast<size_t>(texIdx) >= loadedTextures.size()) {
                    return 0xFFFFFFFF;
                }
                auto texHandle = loadedTextures[texIdx];
                std::lock_guard<std::mutex> lock(assetMutex);
                auto it = textures.find(texHandle.GetID());
                if (it != textures.end() && it->second) {
                    return it->second->GetBindlessSlot();
                }
                return 0xFFFFFFFF;
            };

            for (const auto& matDesc : result.materials) {
                MaterialGPU gpuMat{};
                gpuMat.baseColorFactor = matDesc.baseColorFactor;
                gpuMat.emissiveFactor = matDesc.emissiveFactor;
                gpuMat.roughnessFactor = matDesc.roughnessFactor;
                gpuMat.metallicFactor = matDesc.metallicFactor;
                gpuMat.alphaCutoff = matDesc.alphaCutoff;
                gpuMat.alphaMode = static_cast<uint32_t>(matDesc.alphaMode);
                gpuMat.flags = matDesc.doubleSided ? 1u : 0u;

                gpuMat.albedoSlot = getBindlessSlot(matDesc.albedoTextureIndex);
                gpuMat.normalSlot = getBindlessSlot(matDesc.normalTextureIndex);
                gpuMat.metallicRoughnessSlot = getBindlessSlot(matDesc.metallicRoughnessTextureIndex);
                gpuMat.emissiveSlot = getBindlessSlot(matDesc.emissiveTextureIndex);
                gpuMat.occlusionSlot = getBindlessSlot(matDesc.occlusionTextureIndex);

                gpuMaterials.push_back(gpuMat);
            }

            if (result.materials.empty()) {
                PBRMaterialDesc defaultDesc{};
                defaultDesc.name = "DefaultMaterial";
                result.materials.push_back(std::move(defaultDesc));

                MaterialGPU defaultGpu{};
                defaultGpu.baseColorFactor = glm::vec4(1.0f);
                defaultGpu.roughnessFactor = 1.0f;
                defaultGpu.metallicFactor = 0.0f;
                defaultGpu.albedoSlot = 0xFFFFFFFF;
                defaultGpu.normalSlot = 0xFFFFFFFF;
                defaultGpu.metallicRoughnessSlot = 0xFFFFFFFF;
                defaultGpu.emissiveSlot = 0xFFFFFFFF;
                defaultGpu.occlusionSlot = 0xFFFFFFFF;
                gpuMaterials.push_back(defaultGpu);
            }

            std::string meshName = result.name.empty() ? std::filesystem::path(pathStr).stem().string() : result.name;
            auto createdMesh = std::make_unique<Mesh>(
                std::move(meshName),
                std::move(submeshes),
                std::move(result.materials),
                std::move(gpuMaterials),
                std::move(loadedTextures)
            );

            {
                std::lock_guard<std::mutex> lock(assetMutex);
                meshes[handle.GetID()] = std::move(createdMesh);
                assetStates[handle.GetID()] = AssetState::Ready;
            }
        });

        {
            std::lock_guard<std::mutex> lock(jobMutex);
            activeJobs.push_back(job);
        }

        return handle;
    }

    AssetHandle<VulkanTexture> AssetManager::LoadTextureAsync(std::string_view filePath, bool srgb)
    {
        std::string pathStr(filePath);
        {
            std::lock_guard<std::mutex> lock(assetMutex);
            auto it = pathToTexture.find(pathStr);
            if (it != pathToTexture.end()) {
                return it->second;
            }
        }

        uint32_t index = nextAssetIndex++;
        AssetHandle<VulkanTexture> handle = AssetHandle<VulkanTexture>::Create(index, 1);

        {
            std::lock_guard<std::mutex> lock(assetMutex);
            pathToTexture[pathStr] = handle;
            assetStates[handle.GetID()] = AssetState::Loading;
        }

        ENGINE_ASSERT(jobSystem, "AssetManager: JobSystem is not initialized");
        JobHandle job = jobSystem->Execute([this, handle, pathStr, srgb]() {
            std::unique_ptr<VulkanTexture> texture;
            if (device && memory) {
                texture = TextureLoader::LoadAndUpload(*device, *memory, pathStr, srgb, true);
            }
            std::lock_guard<std::mutex> lock(assetMutex);
            if (texture) {
                if (bindlessHeap) {
                    auto bindlessHandle = bindlessHeap->RegisterTexture(texture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
                    texture->SetBindlessSlot(bindlessHandle.slot);
                }
                textures[handle.GetID()] = std::move(texture);
                assetStates[handle.GetID()] = AssetState::Ready;
            } else {
                assetStates[handle.GetID()] = AssetState::Failed;
            }
        });

        {
            std::lock_guard<std::mutex> lock(jobMutex);
            activeJobs.push_back(job);
        }

        return handle;
    }

    Mesh* AssetManager::GetMesh(AssetHandle<Mesh> handle)
    {
        std::lock_guard<std::mutex> lock(assetMutex);
        auto it = meshes.find(handle.GetID());
        return (it != meshes.end()) ? it->second.get() : nullptr;
    }

    VulkanTexture* AssetManager::GetTexture(AssetHandle<VulkanTexture> handle)
    {
        std::lock_guard<std::mutex> lock(assetMutex);
        auto it = textures.find(handle.GetID());
        return (it != textures.end()) ? it->second.get() : nullptr;
    }

    AssetState AssetManager::GetAssetState(uint64_t handleId) const
    {
        std::lock_guard<std::mutex> lock(assetMutex);
        auto it = assetStates.find(handleId);
        return (it != assetStates.end()) ? it->second : AssetState::Unloaded;
    }
}