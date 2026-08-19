#include "Common/SyntheticMeshGenerator.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace Engine::Test {

    SyntheticMesh SyntheticMeshGenerator::GenerateCube(float halfExtent) {
        SyntheticMesh mesh;
        mesh.aabbMin = glm::vec3(-halfExtent);
        mesh.aabbMax = glm::vec3(halfExtent);
        mesh.boundingSphereCenter = glm::vec3(0.0f);
        mesh.boundingSphereRadius = halfExtent * std::sqrt(3.0f);

        mesh.vertices.reserve(24);
        mesh.indices.reserve(36);

        auto addFace = [&](const glm::vec3& normal, const glm::vec3& tangent, const glm::vec3& bitangent) {
            uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());

            glm::vec3 c = normal * halfExtent;
            glm::vec3 p0 = c - tangent * halfExtent - bitangent * halfExtent;
            glm::vec3 p1 = c + tangent * halfExtent - bitangent * halfExtent;
            glm::vec3 p2 = c + tangent * halfExtent + bitangent * halfExtent;
            glm::vec3 p3 = c - tangent * halfExtent + bitangent * halfExtent;

            SyntheticVertex v0{ p0, normal, glm::vec4(tangent, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(1.0f) };
            SyntheticVertex v1{ p1, normal, glm::vec4(tangent, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(1.0f) };
            SyntheticVertex v2{ p2, normal, glm::vec4(tangent, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(1.0f) };
            SyntheticVertex v3{ p3, normal, glm::vec4(tangent, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(1.0f) };

            mesh.vertices.push_back(v0);
            mesh.vertices.push_back(v1);
            mesh.vertices.push_back(v2);
            mesh.vertices.push_back(v3);

            mesh.indices.push_back(baseIndex + 0);
            mesh.indices.push_back(baseIndex + 1);
            mesh.indices.push_back(baseIndex + 2);
            mesh.indices.push_back(baseIndex + 2);
            mesh.indices.push_back(baseIndex + 3);
            mesh.indices.push_back(baseIndex + 0);
        };

        // +Z, -Z, +X, -X, +Y, -Y
        addFace(glm::vec3( 0,  0,  1), glm::vec3( 1,  0,  0), glm::vec3(0,  1,  0));
        addFace(glm::vec3( 0,  0, -1), glm::vec3(-1,  0,  0), glm::vec3(0,  1,  0));
        addFace(glm::vec3( 1,  0,  0), glm::vec3( 0,  0, -1), glm::vec3(0,  1,  0));
        addFace(glm::vec3(-1,  0,  0), glm::vec3( 0,  0,  1), glm::vec3(0,  1,  0));
        addFace(glm::vec3( 0,  1,  0), glm::vec3( 1,  0,  0), glm::vec3(0,  0, -1));
        addFace(glm::vec3( 0, -1,  0), glm::vec3( 1,  0,  0), glm::vec3(0,  0,  1));

        return mesh;
    }

    SyntheticMesh SyntheticMeshGenerator::GenerateQuad(float halfExtent) {
        SyntheticMesh mesh;
        mesh.aabbMin = glm::vec3(-halfExtent, -halfExtent, 0.0f);
        mesh.aabbMax = glm::vec3(halfExtent, halfExtent, 0.0f);
        mesh.boundingSphereCenter = glm::vec3(0.0f);
        mesh.boundingSphereRadius = halfExtent * std::sqrt(2.0f);

        glm::vec3 normal(0.0f, 0.0f, 1.0f);
        glm::vec4 tangent(1.0f, 0.0f, 0.0f, 1.0f);

        mesh.vertices = {
            { glm::vec3(-halfExtent, -halfExtent, 0.0f), normal, tangent, glm::vec2(0.0f, 0.0f), glm::vec3(1.0f) },
            { glm::vec3( halfExtent, -halfExtent, 0.0f), normal, tangent, glm::vec2(1.0f, 0.0f), glm::vec3(1.0f) },
            { glm::vec3( halfExtent,  halfExtent, 0.0f), normal, tangent, glm::vec2(1.0f, 1.0f), glm::vec3(1.0f) },
            { glm::vec3(-halfExtent,  halfExtent, 0.0f), normal, tangent, glm::vec2(0.0f, 1.0f), glm::vec3(1.0f) }
        };

        mesh.indices = { 0, 1, 2, 2, 3, 0 };
        return mesh;
    }

    SyntheticMesh SyntheticMeshGenerator::GenerateSphere(float radius, uint32_t rings, uint32_t sectors) {
        SyntheticMesh mesh;
        mesh.aabbMin = glm::vec3(-radius);
        mesh.aabbMax = glm::vec3(radius);
        mesh.boundingSphereCenter = glm::vec3(0.0f);
        mesh.boundingSphereRadius = radius;

        rings = std::max(rings, 3u);
        sectors = std::max(sectors, 3u);

        const float R = 1.0f / static_cast<float>(rings - 1);
        const float S = 1.0f / static_cast<float>(sectors - 1);
        const float PI = static_cast<float>(std::numbers::pi);

        for (uint32_t r = 0; r < rings; ++r) {
            for (uint32_t s = 0; s < sectors; ++s) {
                float y = std::sin(-PI / 2.0f + PI * r * R);
                float x = std::cos(2.0f * PI * s * S) * std::sin(PI * r * R);
                float z = std::sin(2.0f * PI * s * S) * std::sin(PI * r * R);

                glm::vec3 norm = glm::normalize(glm::vec3(x, y, z));
                glm::vec3 pos = norm * radius;
                glm::vec2 uv(s * S, r * R);
                glm::vec4 tangent(glm::normalize(glm::vec3(-std::sin(2.0f * PI * s * S), 0.0f, std::cos(2.0f * PI * s * S))), 1.0f);

                mesh.vertices.push_back({ pos, norm, tangent, uv, glm::vec3(1.0f) });
            }
        }

        for (uint32_t r = 0; r < rings - 1; ++r) {
            for (uint32_t s = 0; s < sectors - 1; ++s) {
                uint32_t cur = r * sectors + s;
                uint32_t next = (r + 1) * sectors + s;

                mesh.indices.push_back(cur);
                mesh.indices.push_back(next);
                mesh.indices.push_back(next + 1);

                mesh.indices.push_back(cur);
                mesh.indices.push_back(next + 1);
                mesh.indices.push_back(cur + 1);
            }
        }

        return mesh;
    }

    std::vector<SyntheticMeshlet> SyntheticMeshGenerator::PartitionIntoMeshlets(
        const SyntheticMesh& mesh,
        uint32_t maxVertices,
        uint32_t maxTriangles)
    {
        std::vector<SyntheticMeshlet> meshlets;
        uint32_t totalTriangles = static_cast<uint32_t>(mesh.indices.size() / 3);

        uint32_t triOffset = 0;
        while (triOffset < totalTriangles) {
            uint32_t trisInMeshlet = std::min(maxTriangles, totalTriangles - triOffset);

            SyntheticMeshlet m;
            m.triangleOffset = triOffset;
            m.triangleCount = trisInMeshlet;
            m.vertexOffset = triOffset * 3; // simplified synthetic indexing
            m.vertexCount = std::min(maxVertices, trisInMeshlet * 3);

            // Compute approximate bounding sphere for meshlet
            glm::vec3 minP(1e9f);
            glm::vec3 maxP(-1e9f);
            glm::vec3 avgNormal(0.0f);

            for (uint32_t t = 0; t < trisInMeshlet; ++t) {
                uint32_t idx0 = mesh.indices[(triOffset + t) * 3 + 0];
                uint32_t idx1 = mesh.indices[(triOffset + t) * 3 + 1];
                uint32_t idx2 = mesh.indices[(triOffset + t) * 3 + 2];

                minP = glm::min(minP, mesh.vertices[idx0].position);
                minP = glm::min(minP, mesh.vertices[idx1].position);
                minP = glm::min(minP, mesh.vertices[idx2].position);

                maxP = glm::max(maxP, mesh.vertices[idx0].position);
                maxP = glm::max(maxP, mesh.vertices[idx1].position);
                maxP = glm::max(maxP, mesh.vertices[idx2].position);

                avgNormal += mesh.vertices[idx0].normal + mesh.vertices[idx1].normal + mesh.vertices[idx2].normal;
            }

            m.center = (minP + maxP) * 0.5f;
            m.radius = glm::length(maxP - minP) * 0.5f;
            m.coneAxis = glm::length(avgNormal) > 1e-4f ? glm::normalize(avgNormal) : glm::vec3(0.0f, 1.0f, 0.0f);
            m.coneCutoff = 0.5f; // default cone angle cutoff

            meshlets.push_back(m);
            triOffset += trisInMeshlet;
        }

        return meshlets;
    }

} // namespace Engine::Test
