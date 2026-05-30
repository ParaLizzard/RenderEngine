#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "Scene/Texture.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/Model.h"
#include "Scene/GameObject.h"
#include "Core/Device.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <optional>
#include <vector>

namespace Engine
{
    class LoaderGLTF
    {
    public:
        // Main entry point.
        // outTextures receives ownership of all uploaded GPU textures so they
        // stay alive for the lifetime of the scene (store in Application).
        static std::vector<GameObject> loadObjectGLTF(
            Device&                        device,
            const std::filesystem::path&   filePath,
            Model&                         megaBuffer,
            ResourceHeap&                  resourceHeap,
            std::vector<Texture2D>&        outTextures);

    private:
        static fastgltf::Asset loadAsset(const std::filesystem::path& filePath);

        // Upload all images in the asset (deduplication: one upload per image index).
        // Returns a parallel array: gltfImageIndex -> bindless heap slot.
        static std::vector<uint32_t> uploadImages(
            Device&                        device,
            fastgltf::Asset&               asset,
            const std::filesystem::path&   assetDir,
            ResourceHeap&                  resourceHeap,
            std::vector<Texture2D>&        outTextures);

        // Construct ResourceHeap::MaterialData for every GLTF material.
        // Returns a parallel array: [0] = default sentinel, [i+1] = gltfMaterialIndex i.
        static std::vector<uint32_t> buildMaterials(
            fastgltf::Asset&               asset,
            const std::vector<uint32_t>&   imageSlots,
            ResourceHeap&                  resourceHeap);

        // Upload vertices + indices for one GLTF mesh (all primitives merged).
        // Writes materialID into every vertex.texId.
        static std::optional<Model::SubMesh> loadMesh(
            fastgltf::Asset&               asset,
            fastgltf::Mesh&                mesh,
            Model&                         megaBuffer,
            const std::vector<uint32_t>&   materialIndexToID);
    };
}