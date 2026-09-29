#include "GeometryBufferPool.h"

#include "Core/Assert.h"
#include "Core/EngineConstants.h"

#include "Vulkan/VulkanDevice.h"

namespace Engine
{
    GeometryBufferPool::GeometryBufferPool(VulkanDevice &device,
        VulkanMemory &memory,
        size_t maxVertices,
        size_t maxIndices,
        size_t maxMeshlets):
        device(device),
        memory(memory),
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

    void GeometryBufferPool::UploadSubmesh(
        const SubmeshGPUAllocation& allocation,
        std::span<const VertexPositionGPU> positions,
        std::span<const VertexAttributeGPU> attributes,
        std::span<const uint32_t> indices,
        std::span<const MeshletGPU> meshlets,
        std::span<const uint32_t> meshletVertices,
        std::span<const uint8_t> meshletTriangles)
    {
        VkDeviceSize posSize = positions.size_bytes();
        VkDeviceSize attrSize = attributes.size_bytes();
        VkDeviceSize idxSize = indices.size_bytes();
        VkDeviceSize meshletSize = meshlets.size_bytes();
        VkDeviceSize vertSize = meshletVertices.size_bytes();
        VkDeviceSize triSize = meshletTriangles.size_bytes();

        VkDeviceSize totalSize = posSize + attrSize + idxSize + meshletSize + vertSize + triSize;
        if (totalSize == 0) {
            return;
        }

        BufferDesc stgDesc{
            .debugName = "GeometryUploadStaging",
            .size = totalSize,
            .usage = BufferUsage::STAGING,
            .memoryUsage = MemoryUsage::CPU_TO_GPU
        };
        VulkanBuffer staging(device, memory, stgDesc);

        VkDeviceSize currentOffset = 0;
        VkDeviceSize posOffset = currentOffset;
        if (posSize > 0) {
            staging.UpdateData(positions.data(), posSize, currentOffset);
            currentOffset += posSize;
        }

        VkDeviceSize attrOffset = currentOffset;
        if (attrSize > 0) {
            staging.UpdateData(attributes.data(), attrSize, currentOffset);
            currentOffset += attrSize;
        }

        VkDeviceSize idxOffset = currentOffset;
        if (idxSize > 0) {
            staging.UpdateData(indices.data(), idxSize, currentOffset);
            currentOffset += idxSize;
        }

        VkDeviceSize meshletOffset = currentOffset;
        if (meshletSize > 0) {
            staging.UpdateData(meshlets.data(), meshletSize, currentOffset);
            currentOffset += meshletSize;
        }

        VkDeviceSize vertOffset = currentOffset;
        if (vertSize > 0) {
            staging.UpdateData(meshletVertices.data(), vertSize, currentOffset);
            currentOffset += vertSize;
        }

        VkDeviceSize triOffset = currentOffset;
        if (triSize > 0) {
            staging.UpdateData(meshletTriangles.data(), triSize, currentOffset);
            currentOffset += triSize;
        }

        device.ExecuteImmediate(QueueType::Graphics, [&](VkCommandBuffer cmd) {
            if (posSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = posOffset,
                    .dstOffset = allocation.vertexOffset * sizeof(VertexPositionGPU),
                    .size = posSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), positionBuffer->GetHandle(), 1, &copy);
            }
            if (attrSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = attrOffset,
                    .dstOffset = allocation.vertexOffset * sizeof(VertexAttributeGPU),
                    .size = attrSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), attributeBuffer->GetHandle(), 1, &copy);
            }
            if (idxSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = idxOffset,
                    .dstOffset = allocation.indexOffset * sizeof(uint32_t),
                    .size = idxSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), indexBuffer->GetHandle(), 1, &copy);
            }
            if (meshletSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = meshletOffset,
                    .dstOffset = allocation.meshletOffset * sizeof(MeshletGPU),
                    .size = meshletSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), meshletBuffer->GetHandle(), 1, &copy);
            }
            if (vertSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = vertOffset,
                    .dstOffset = allocation.meshletVertexOffset * sizeof(uint32_t),
                    .size = vertSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), meshletVerticesBuffer->GetHandle(), 1, &copy);
            }
            if (triSize > 0) {
                VkBufferCopy copy{
                    .srcOffset = triOffset,
                    .dstOffset = allocation.meshletTriangleOffset * sizeof(uint8_t),
                    .size = triSize
                };
                vkCmdCopyBuffer(cmd, staging.GetHandle(), meshletTrianglesBuffer->GetHandle(), 1, &copy);
            }
        });
    }
}