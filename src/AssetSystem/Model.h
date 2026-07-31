#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <meshoptimizer.h>


namespace Engine {
    class Buffer;
    class Device;
    class Model
    {
    public:
        struct SubMesh
        {
            uint32_t indexCount = 0;
            uint32_t firstIndex = 0;
            int32_t vertexOffset = 0;
            uint32_t baseMeshlet = 0;
            uint32_t meshletCount = 0;
        };

        struct Meshlet {
            glm::vec3 center;
            float radius;

            // Quantized normal cone
            signed char cone_axis[3];
            signed char cone_cutoff;

            glm::uint vertexOffset;
            glm::uint indexOffset;
            glm::uint vertexCount;
            glm::uint triangleCount;
        };

        struct VertexPosition
        {
            glm::vec3 position {};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct VertexAttribute
        {
            glm::vec3 color {};
            glm::vec3 normal {};
            glm::vec2 uv {};
            glm::vec4 tangent {};
            uint32_t texId {0};
        };

        Model(Device &device);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        SubMesh registerMesh(const std::vector<VertexPosition> &positions,
                             const std::vector<VertexAttribute> &attributes,
                             const std::vector<uint32_t> &indices);

        void uploadToGPU();

        void bind(VkCommandBuffer commandBuffer);

        void bindPositionOnly(VkCommandBuffer commandBuffer);

        std::shared_ptr<Buffer> getPositionBuffer() const
        {
            return positionBuffer ? positionBuffer : nullptr;
        }
        std::shared_ptr<Buffer> getAttributeBuffer() const
        {
            return attributeBuffer ? attributeBuffer : nullptr;
        }
        std::shared_ptr<Buffer> getIndexBuffer() const
        {
            return indexBuffer ? indexBuffer : nullptr;
        }
        std::shared_ptr<Buffer> getMeshletBuffer() const
        {
            return meshletBuffer ? meshletBuffer : nullptr;
        }
        std::shared_ptr<Buffer> getMeshletVerticesBuffer() const
        {
            return meshletVerticesBuffer ? meshletVerticesBuffer : nullptr;
        }
        std::shared_ptr<Buffer> getMeshletTrianglesBuffer() const
        {
            return meshletTrianglesBuffer ? meshletTrianglesBuffer : nullptr;
        }

        [[nodiscard]] uint32_t getVertexCount() const {return  totalAllocatedVertices;}
        [[nodiscard]] uint32_t getIndexCount() const {return  totalAllocatedIndices;}

    private:
        Device &device;

        uint32_t totalAllocatedVertices = 0;
        uint32_t totalAllocatedIndices = 0;

        // CPU staging arrays
        std::vector<VertexPosition> cpuPositions;
        std::vector<VertexAttribute> cpuAttributes;
        std::vector<uint32_t> cpuIndices;
        std::vector<Meshlet> cpuMeshlets;
        std::vector<uint32_t> cpuMeshletVertices;
        std::vector<uint8_t> cpuMeshletTriangles;

        // Unified GPU-Only
        std::shared_ptr<Buffer> positionBuffer;
        std::shared_ptr<Buffer> attributeBuffer;
        std::shared_ptr<Buffer> indexBuffer;
        std::shared_ptr<Buffer> meshletBuffer;
        std::shared_ptr<Buffer> meshletVerticesBuffer;
        std::shared_ptr<Buffer> meshletTrianglesBuffer;
    };
} // namespace Engine
