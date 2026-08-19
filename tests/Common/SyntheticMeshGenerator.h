#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Engine::Test {

    struct SyntheticVertex {
        glm::vec3 position{ 0.0f };
        glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
        glm::vec4 tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
        glm::vec2 uv{ 0.0f };
        glm::vec3 color{ 1.0f };
    };

    struct SyntheticMeshlet {
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t triangleOffset = 0;
        uint32_t triangleCount = 0;
        glm::vec3 center{ 0.0f };
        float radius = 0.0f;
        glm::vec3 coneAxis{ 0.0f, 1.0f, 0.0f };
        float coneCutoff = 1.0f; // cos(angle)
    };

    struct SyntheticMesh {
        std::vector<SyntheticVertex> vertices;
        std::vector<uint32_t> indices;
        glm::vec3 aabbMin{ 0.0f };
        glm::vec3 aabbMax{ 0.0f };
        glm::vec3 boundingSphereCenter{ 0.0f };
        float boundingSphereRadius = 0.0f;
    };

    class SyntheticMeshGenerator {
    public:
        // Generates an axis-aligned cube [-halfExtent, +halfExtent] with 24 vertices (4 per face) and 36 indices
        static SyntheticMesh GenerateCube(float halfExtent = 1.0f);

        // Generates a flat XY quad with 4 vertices and 6 indices
        static SyntheticMesh GenerateQuad(float halfExtent = 1.0f);

        // Generates a UV sphere with specified rings and sectors
        static SyntheticMesh GenerateSphere(float radius = 1.0f, uint32_t rings = 16, uint32_t sectors = 16);

        // Clusters synthetic mesh triangles into meshlet chunks adhering to vertex and triangle limits
        static std::vector<SyntheticMeshlet> PartitionIntoMeshlets(
            const SyntheticMesh& mesh,
            uint32_t maxVertices = 64,
            uint32_t maxTriangles = 124);
    };

} // namespace Engine::Test
