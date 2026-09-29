#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>
#include "AssetSystem/GeometryBufferPool.h"
#include "AssetSystem/Material.h"
#include "AssetSystem/AssetHandle.h"
#include "Vulkan/VulkanTexture.h"

namespace Engine {
    struct Submesh {
        std::string name;
        SubmeshGPUAllocation gpuAllocation;
        uint32_t materialIndex = 0;
        glm::vec4 boundingSphere{ 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec3 aabbMin{ -1.0f };
        glm::vec3 aabbMax{ 1.0f };
    };

    class Mesh {
    public:
        Mesh(std::string name,
             std::vector<Submesh> submeshes,
             std::vector<PBRMaterialDesc> materials = {},
             std::vector<MaterialGPU> gpuMaterials = {},
             std::vector<AssetHandle<VulkanTexture>> textures = {});
        ~Mesh() = default;

        ENGINE_NODISCARD std::string_view GetName() const noexcept { return name; }
        ENGINE_NODISCARD const std::vector<Submesh>& GetSubmeshes() const noexcept { return submeshes; }
        ENGINE_NODISCARD size_t GetSubmeshCount() const noexcept { return submeshes.size(); }

        ENGINE_NODISCARD const std::vector<PBRMaterialDesc>& GetMaterials() const noexcept { return materials; }
        ENGINE_NODISCARD const std::vector<MaterialGPU>& GetGPUMaterials() const noexcept { return gpuMaterials; }
        ENGINE_NODISCARD const std::vector<AssetHandle<VulkanTexture>>& GetTextures() const noexcept { return textures; }

        ENGINE_NODISCARD const PBRMaterialDesc* GetMaterial(size_t index) const noexcept {
            return index < materials.size() ? &materials[index] : nullptr;
        }

        ENGINE_NODISCARD const MaterialGPU* GetGPUMaterial(size_t index) const noexcept {
            return index < gpuMaterials.size() ? &gpuMaterials[index] : nullptr;
        }

        ENGINE_NODISCARD uint32_t GetTotalMeshletCount() const noexcept {
            uint32_t count = 0;
            for (const auto& sub : submeshes) count += sub.gpuAllocation.meshletCount;
            return count;
        }

        ENGINE_NODISCARD const glm::vec4& GetBoundingSphere() const noexcept { return boundingSphere; }

    private:
        std::string name;
        std::vector<Submesh> submeshes;
        std::vector<PBRMaterialDesc> materials;
        std::vector<MaterialGPU> gpuMaterials;
        std::vector<AssetHandle<VulkanTexture>> textures;
        glm::vec4 boundingSphere{ 0.0f, 0.0f, 0.0f, 1.0f };
    };
}