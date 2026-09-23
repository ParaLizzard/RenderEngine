#include "GeometryBufferPool.h"

#include "Core/Assert.h"
#include "Core/EngineConstants.h"

namespace Engine
{
    GeometryBufferPool::GeometryBufferPool(VulkanDevice &device,
        VulkanMemory &memory,
        size_t maxVertices,
        size_t maxIndices,
        size_t maxMeshlets):
        maxVertices(maxVertices),
        maxIndices(maxIndices),
        maxMeshlets(maxMeshlets),
        maxMeshletVertices(maxMeshlets * Constants::Meshlet::MAX_MESHLET_VERTICES),
        maxMeshletTriangles(maxMeshlets * Constants::Meshlet::MAX_MESHLET_TRIANGLES * 3)
    {
        VkDeviceSize positionSize = maxVertices * sizeof(VertexPositionGPU);
        VkDeviceSize indexSize = maxIndices * sizeof(uint32_t);
        VkDeviceSize attributeSize = maxVertices * sizeof(VertexAttributeGPU);
        VkDeviceSize meshletSize = maxMeshlets * sizeof(MeshletGPU);
        VkDeviceSize meshletVerticesSize = maxMeshletVertices * sizeof(uint32_t);
        VkDeviceSize meshletTrianglesSize = maxMeshletTriangles * sizeof(uint8_t);

        BufferDesc positionDesc = {
            .debugName = "PositionBuffer",
            .size = positionSize,
            .usage = BufferUsage::VERTEX | BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        positionBuffer = std::make_unique<VulkanBuffer>(device, memory, positionDesc);

        BufferDesc indexDesc = {
            .debugName = "IndexBuffer",
            .size = indexSize,
            .usage = BufferUsage::INDEX | BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        indexBuffer = std::make_unique<VulkanBuffer>(device, memory, indexDesc);

        BufferDesc attrDesc = {
            .debugName = "AttributeBuffer",
            .size = attributeSize,
            .usage = BufferUsage::VERTEX | BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        attributeBuffer = std::make_unique<VulkanBuffer>(device, memory, attrDesc);

        BufferDesc meshletDesc = {
            .debugName = "MeshletBuffer",
            .size = meshletSize,
            .usage = BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        meshletBuffer = std::make_unique<VulkanBuffer>(device, memory, meshletDesc);

        BufferDesc meshletVerticeDesc = {
            .debugName = "MeshletVerticeBuffer",
            .size = meshletVerticesSize,
            .usage = BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        meshletVerticesBuffer = std::make_unique<VulkanBuffer>(device, memory, meshletVerticeDesc);

        BufferDesc meshletTriangleDesc = {
            .debugName = "MeshletTriangleBuffer",
            .size = meshletTrianglesSize,
            .usage = BufferUsage::STORAGE | BufferUsage::TRANSFER_DST,
            .memoryUsage = MemoryUsage::GPU_ONLY,
        };
        meshletTrianglesBuffer = std::make_unique<VulkanBuffer>(device, memory, meshletTriangleDesc);
    }

    GeometryBufferPool::~GeometryBufferPool() = default;

    SubmeshGPUAllocation GeometryBufferPool::AllocateSubmesh(uint32_t vertexCount,
        uint32_t indexCount,
        uint32_t meshletCount,
        uint32_t meshletVertexCount,
        uint32_t meshletTriangleCount)
    {
        std::lock_guard<std::mutex> lock(allocationMutex);

        ENGINE_ASSERT(currentVertexOffset + vertexCount <= maxVertices, "GeometryBufferPool: Vertex buffer overflow");
        ENGINE_ASSERT(currentIndexOffset + indexCount <= maxIndices, "GeometryBufferPool: Index buffer overflow");
        ENGINE_ASSERT(currentMeshletOffset + meshletCount <= maxMeshlets, "GeometryBufferPool: Meshlet buffer overflow");
        ENGINE_ASSERT(currentMeshletVertexOffset + meshletVertexCount <= maxMeshletVertices, "GeometryBufferPool: Meshlet vertex buffer overflow");
        ENGINE_ASSERT(currentMeshletTriangleOffset + meshletTriangleCount <= maxMeshletTriangles, "GeometryBufferPool: Meshlet triangle buffer overflow");

        SubmeshGPUAllocation allocation{};
        allocation.vertexOffset = currentVertexOffset;
        allocation.vertexCount = vertexCount;
        allocation.indexOffset = currentIndexOffset;
        allocation.indexCount = indexCount;
        allocation.meshletOffset = currentMeshletOffset;
        allocation.meshletCount = meshletCount;
        allocation.meshletVertexOffset = currentMeshletVertexOffset;
        allocation.meshletTriangleOffset = currentMeshletTriangleOffset;

        currentVertexOffset += vertexCount;
        currentIndexOffset += indexCount;
        currentMeshletOffset += meshletCount;
        currentMeshletVertexOffset += meshletVertexCount;
        currentMeshletTriangleOffset += meshletTriangleCount;

        return allocation;
    }

    void GeometryBufferPool::FreeSubmesh(const SubmeshGPUAllocation &allocation)
    {
        std::lock_guard<std::mutex> lock(allocationMutex);
    }
}