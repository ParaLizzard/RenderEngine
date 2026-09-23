#pragma once
#include "Vulkan/VulkanBuffer.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/vec3.hpp>
#include <mutex>

namespace Engine {
    struct MeshletGPU {
        float centerX, centerY, centerZ, radius;
        uint32_t coneAxisCutoff;
        uint32_t vertexOffset;
        uint32_t triangleOffset;
        uint32_t vertexCount;
        uint32_t triangleCount;
    };

    struct VertexPositionGPU {
        glm::vec3 position;
    };

    struct VertexAttributeGPU {
        uint32_t tangentLo;
        uint32_t tangentHi;
        uint32_t uv;
        uint32_t normalOct;
    };

    struct SubmeshGPUAllocation {
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t meshletOffset = 0;
        uint32_t meshletCount = 0;
        uint32_t meshletVertexOffset = 0;
        uint32_t meshletTriangleOffset = 0;
    };

    class GeometryBufferPool {
    public:
        GeometryBufferPool(VulkanDevice& device, VulkanMemory& memory,
                           size_t maxVertices = 2'000'000,
                           size_t maxIndices = 6'000'000,
                           size_t maxMeshlets = 500'000);
        ~GeometryBufferPool();

        SubmeshGPUAllocation AllocateSubmesh(uint32_t vertexCount, uint32_t indexCount, uint32_t meshletCount, uint32_t meshletVertexCount, uint32_t meshletTriangleCount);
        void FreeSubmesh(const SubmeshGPUAllocation& allocation);

        ENGINE_NODISCARD VkDeviceAddress GetPositionBufferAddress() const noexcept { return positionBuffer->GetDeviceAddress(); }
        ENGINE_NODISCARD VkDeviceAddress GetAttributeBufferAddress() const noexcept { return attributeBuffer->GetDeviceAddress(); }
        ENGINE_NODISCARD VkDeviceAddress GetIndexBufferAddress() const noexcept { return indexBuffer->GetDeviceAddress(); }
        ENGINE_NODISCARD VkDeviceAddress GetMeshletBufferAddress() const noexcept { return meshletBuffer->GetDeviceAddress(); }
        ENGINE_NODISCARD VkDeviceAddress GetMeshletVerticesAddress() const noexcept { return meshletVerticesBuffer->GetDeviceAddress(); }
        ENGINE_NODISCARD VkDeviceAddress GetMeshletTrianglesAddress() const noexcept { return meshletTrianglesBuffer->GetDeviceAddress(); }

        ENGINE_NODISCARD uint32_t GetAllocatedVertices() const noexcept { return currentVertexOffset; }
        ENGINE_NODISCARD uint32_t GetAllocatedIndices() const noexcept { return currentIndexOffset; }
        ENGINE_NODISCARD uint32_t GetAllocatedMeshlets() const noexcept { return currentMeshletOffset; }

    private:
        std::unique_ptr<VulkanBuffer> positionBuffer;
        std::unique_ptr<VulkanBuffer> attributeBuffer;
        std::unique_ptr<VulkanBuffer> indexBuffer;
        std::unique_ptr<VulkanBuffer> meshletBuffer;
        std::unique_ptr<VulkanBuffer> meshletVerticesBuffer;
        std::unique_ptr<VulkanBuffer> meshletTrianglesBuffer;

        size_t maxVertices = 0;
        size_t maxIndices = 0;
        size_t maxMeshlets = 0;
        size_t maxMeshletVertices = 0;
        size_t maxMeshletTriangles = 0;

        uint32_t currentVertexOffset = 0;
        uint32_t currentIndexOffset = 0;
        uint32_t currentMeshletOffset = 0;
        uint32_t currentMeshletVertexOffset = 0;
        uint32_t currentMeshletTriangleOffset = 0;

        std::mutex allocationMutex;
    };
}