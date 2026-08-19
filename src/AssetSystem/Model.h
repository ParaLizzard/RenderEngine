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
            uint32_t materialIndex = 0;
        };

#pragma pack(push, 4)
        struct Meshlet {
            float center_x;
            float center_y;
            float center_z;
            float radius;

            uint32_t cone_axis_cutoff;

            uint32_t vertexOffset;
            uint32_t indexOffset;
            uint32_t vertexCount;
            uint32_t triangleCount;
        };
#pragma pack(pop)


        struct VertexPosition
        {
            glm::vec3 position {};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct VertexAttribute
        {
            uint32_t tangent_lo {};
            uint32_t tangent_hi {};
            uint32_t uv {};
            uint32_t normal_oct {};
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
        [[nodiscard]] uint32_t getMeshletCount() const {return  totalAllocatedMeshlets;}

    private:
        Device &device;

        uint32_t totalAllocatedVertices = 0;
        uint32_t totalAllocatedIndices = 0;
        uint32_t totalAllocatedMeshlets = 0;
        uint32_t totalAllocatedMeshletVertices = 0;
        uint32_t totalAllocatedMeshletTriangles = 0;

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
