#include "Mesh.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <glm/glm.hpp>

namespace Engine
{
    Mesh::Mesh(std::string name,
               std::vector<Submesh> submeshes,
               std::vector<PBRMaterialDesc> materials,
               std::vector<MaterialGPU> gpuMaterials,
               std::vector<AssetHandle<VulkanTexture>> textures)
        : name(std::move(name))
        , submeshes(std::move(submeshes))
        , materials(std::move(materials))
        , gpuMaterials(std::move(gpuMaterials))
        , textures(std::move(textures))
    {
        if (this->submeshes.empty()) {
            boundingSphere = glm::vec4(0.0f);
            return;
        }

        if (this->submeshes.size() == 1) {
            boundingSphere = this->submeshes[0].boundingSphere;
            return;
        }

        glm::vec3 minAABB(std::numeric_limits<float>::max());
        glm::vec3 maxAABB(std::numeric_limits<float>::lowest());

        for (const auto& submesh : this->submeshes) {
            minAABB = glm::min(minAABB, submesh.aabbMin);
            maxAABB = glm::max(maxAABB, submesh.aabbMax);
        }

        glm::vec3 center = (minAABB + maxAABB) * 0.5f;
        float radius = glm::distance(center, maxAABB);

        for (const auto& submesh : this->submeshes) {
            glm::vec3 subCenter = glm::vec3(submesh.boundingSphere);
            float subRadius = submesh.boundingSphere.w;
            radius = glm::max(radius, glm::distance(center, subCenter) + subRadius);
        }

        boundingSphere = glm::vec4(center, radius);
    }
} // namespace Engine