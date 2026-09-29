#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AssetSystem/GeometryBufferPool.h"
#include "AssetSystem/Material.h"
#include "Threading/JobSystem.h"

namespace Engine {

    struct ParsedTextureSource {
        std::string name;
        std::string uri;
        std::vector<uint8_t> embeddedData;
        std::string mimeType;
        bool isSRGB = false;
        bool isKTX2 = false;
    };

    struct ParsedSubmeshData {
        std::string name;

        std::vector<VertexPositionGPU> positions;
        std::vector<VertexAttributeGPU> attributes;
        std::vector<uint32_t> indices;

        std::vector<MeshletGPU> meshlets;
        std::vector<uint32_t> meshletVertices;
        std::vector<uint8_t> meshletTriangles;

        uint32_t materialIndex = 0;

        glm::vec3 aabbMin{ -1.0f };
        glm::vec3 aabbMax{ 1.0f };
        glm::vec4 boundingSphere{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct ParsedMeshData {
        std::string name;
        std::vector<ParsedSubmeshData> submeshes;

        glm::vec3 aabbMin{ -1.0f };
        glm::vec3 aabbMax{ 1.0f };
        glm::vec4 boundingSphere{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    enum class ParsedLightType : uint32_t {
        Directional = 0,
        Point       = 1,
        Spot        = 2
    };

    struct ParsedLightData {
        std::string name;
        ParsedLightType type = ParsedLightType::Directional;
        glm::vec3 color{ 1.0f };
        float intensity = 1.0f;
        float range = 0.0f;
        float innerConeAngle = 0.0f;
        float outerConeAngle = 0.785f;
    };

    struct ParsedNodeData {
        std::string name;

        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
        glm::mat4 localMatrix{ 1.0f };

        std::optional<uint32_t> meshIndex;
        std::optional<uint32_t> lightIndex;
        std::optional<uint32_t> cameraIndex;

        int32_t parentIndex = -1;
        std::vector<uint32_t> childrenIndices;
    };

    struct ParsedSceneResult {
        std::string name;
        std::filesystem::path sourcePath;

        std::vector<ParsedMeshData> meshes;
        std::vector<PBRMaterialDesc> materials;
        std::vector<ParsedTextureSource> textures;
        std::vector<ParsedLightData> lights;

        std::vector<ParsedNodeData> nodes;
        std::vector<uint32_t> rootNodeIndices;

        bool success = false;
        std::string errorMessage;

        ENGINE_NODISCARD bool HasHierarchy() const noexcept { return !nodes.empty(); }
        ENGINE_NODISCARD size_t GetTotalMeshCount() const noexcept { return meshes.size(); }
        ENGINE_NODISCARD size_t GetTotalInstanceCount() const noexcept {
            size_t count = 0;
            for (const auto& node : nodes) {
                if (node.meshIndex.has_value()) ++count;
            }
            return count;
        }
        ENGINE_NODISCARD size_t GetTotalMeshletCount() const noexcept {
            size_t count = 0;
            for (const auto& mesh : meshes) {
                for (const auto& sub : mesh.submeshes) count += sub.meshlets.size();
            }
            return count;
        }
    };

    class GLTFLoader {
    public:
        static JobHandle LoadSceneAsync(JobSystem& jobSystem,
                                        const std::filesystem::path& path,
                                        std::function<void(ParsedSceneResult&&)> onComplete);

        static ParsedSceneResult LoadScene(const std::filesystem::path& path, JobSystem* jobSystem = nullptr);
    };


    static inline glm::vec2 EncodeOctNormal(glm::vec3 n)
    {
        float len = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
        if (len > 1e-6f) n /= len;
        glm::vec2 res = (n.z >= 0.0f)
            ? glm::vec2(n.x, n.y)
            : (glm::vec2(1.0f) - glm::abs(glm::vec2(n.y, n.x))) * glm::sign(glm::vec2(n.x, n.y));
        return res * 0.5f + 0.5f;
    }

    static inline uint32_t PackConeAxisCutoff(const float axis[3], float cutoff)
    {
        int8_t coneX = static_cast<int8_t>(std::clamp(axis[0] * 127.0f, -127.0f, 127.0f));
        int8_t coneY = static_cast<int8_t>(std::clamp(axis[1] * 127.0f, -127.0f, 127.0f));
        int8_t coneZ = static_cast<int8_t>(std::clamp(axis[2] * 127.0f, -127.0f, 127.0f));
        int8_t coneCutoff = static_cast<int8_t>(std::clamp(cutoff * 127.0f, -127.0f, 127.0f));

        return (static_cast<uint32_t>(static_cast<uint8_t>(coneX)))
             | (static_cast<uint32_t>(static_cast<uint8_t>(coneY)) << 8)
             | (static_cast<uint32_t>(static_cast<uint8_t>(coneZ)) << 16)
             | (static_cast<uint32_t>(static_cast<uint8_t>(coneCutoff)) << 24);
    }

    static inline void ComputeBoundingVolumes(
        const std::vector<VertexPositionGPU> &positions,
        glm::vec3 &outMin,
        glm::vec3 &outMax,
        glm::vec4 &outSphere)
    {
        if (positions.empty()) {
            outMin = glm::vec3(0.0f);
            outMax = glm::vec3(0.0f);
            outSphere = glm::vec4(0.0f);
            return;
        }

        glm::vec3 minAABB(std::numeric_limits<float>::max());
        glm::vec3 maxAABB(std::numeric_limits<float>::lowest());
        for (const auto &v: positions) {
            minAABB = glm::min(minAABB, v.position);
            maxAABB = glm::max(maxAABB, v.position);
        }
        outMin = minAABB;
        outMax = maxAABB;

        glm::vec3 center = (minAABB + maxAABB) * 0.5f;
        float radius = 0.0f;
        for (const auto &v: positions) {
            radius = glm::max(radius, glm::distance(center, v.position));
        }
        outSphere = glm::vec4(center, radius);
    }

    static inline void ComputeMeshBoundingVolumes(
        const std::vector<ParsedSubmeshData> &submeshes,
        glm::vec3 &outMin,
        glm::vec3 &outMax,
        glm::vec4 &outSphere)
    {
        if (submeshes.empty()) {
            outMin = glm::vec3(0.0f);
            outMax = glm::vec3(0.0f);
            outSphere = glm::vec4(0.0f);
            return;
        }
        if (submeshes.size() == 1) {
            outMin = submeshes[0].aabbMin;
            outMax = submeshes[0].aabbMax;
            outSphere = submeshes[0].boundingSphere;
            return;
        }

        glm::vec3 minAABB(std::numeric_limits<float>::max());
        glm::vec3 maxAABB(std::numeric_limits<float>::lowest());
        for (const auto &sub: submeshes) {
            minAABB = glm::min(minAABB, sub.aabbMin);
            maxAABB = glm::max(maxAABB, sub.aabbMax);
        }
        outMin = minAABB;
        outMax = maxAABB;

        glm::vec3 center = (minAABB + maxAABB) * 0.5f;
        float radius = glm::distance(center, maxAABB);
        for (const auto &sub: submeshes) {
            glm::vec3 subCenter = glm::vec3(sub.boundingSphere);
            float subRadius = sub.boundingSphere.w;
            radius = glm::max(radius, glm::distance(center, subCenter) + subRadius);
        }
        outSphere = glm::vec4(center, radius);
    }




} // namespace Engine
