#include "AssetSystem/Model.h"


#include "Core/EngineConstants.h"
#include "Core/Log.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Device.h"


namespace Engine {
    Model::Model(Device &device): device(device)
    {}

    Model::~Model()
    {
        vkDeviceWaitIdle(device.getDevice());
    }

    std::vector<VkVertexInputBindingDescription> Model::VertexPosition::getBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(2);

        // Binding 0: Position
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(VertexPosition);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Binding 1: Attributes
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(VertexAttribute);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Model::VertexPosition::getAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(VertexPosition, position);

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attributeDescriptions[1].offset = offsetof(VertexAttribute, tangent_lo);

        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R16G16_SFLOAT;
        attributeDescriptions[2].offset = offsetof(VertexAttribute, uv);

        attributeDescriptions[3].binding = 1;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R16G16_SFLOAT;
        attributeDescriptions[3].offset = offsetof(VertexAttribute, normal_oct);

        return attributeDescriptions;
    }

    Model::SubMesh Model::registerMesh(const std::vector<VertexPosition> &positions,
                                       const std::vector<VertexAttribute> &attributes,
                                       const std::vector<uint32_t> &indices)
    {


        SubMesh subMesh {};
        subMesh.indexCount = indices.size();
        subMesh.firstIndex = totalAllocatedIndices + cpuIndices.size();
        subMesh.vertexOffset = totalAllocatedVertices + cpuPositions.size();

        uint32_t maxMeshletAmount = meshopt_buildMeshletsBound(indices.size(), Constants::MAX_MESHLET_VERTICES, Constants::MAX_MESHLET_TRIANGLES);

        std::vector<meshopt_Meshlet> finalMeshlets(maxMeshletAmount);
        std::vector<unsigned int> vertices(maxMeshletAmount * Constants::MAX_MESHLET_VERTICES);
        std::vector<unsigned char> triangles(maxMeshletAmount * Constants::MAX_MESHLET_TRIANGLES * 3);

        uint32_t totalMeshlets = meshopt_buildMeshlets(
            finalMeshlets.data(),
            vertices.data(),
            triangles.data(),
            indices.data(),
            indices.size(),
            &positions[0].position.x,
            positions.size(),
            sizeof(VertexPosition),
            Constants::MAX_MESHLET_VERTICES,
            Constants::MAX_MESHLET_TRIANGLES,
            Constants::CONE_WEIGHT
            );

        finalMeshlets.resize(totalMeshlets);
        
        subMesh.baseMeshlet = totalAllocatedMeshlets + cpuMeshlets.size();
        subMesh.meshletCount = totalMeshlets;
        
        uint32_t meshletVerticesOffset = totalAllocatedMeshletVertices + cpuMeshletVertices.size();
        uint32_t meshletTrianglesOffset = totalAllocatedMeshletTriangles + cpuMeshletTriangles.size();

        for (auto& m : finalMeshlets) {
            meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                &vertices[m.vertex_offset],
                &triangles[m.triangle_offset],
                m.triangle_count,
                &positions[0].position.x,
                positions.size(),
                sizeof(VertexPosition)
            );

            Meshlet meshlet {};
            meshlet.center_x = bounds.center[0];
            meshlet.center_y = bounds.center[1];
            meshlet.center_z = bounds.center[2];
            meshlet.radius   = bounds.radius;

            uint8_t c0 = static_cast<uint8_t>(bounds.cone_axis_s8[0]);
            uint8_t c1 = static_cast<uint8_t>(bounds.cone_axis_s8[1]);
            uint8_t c2 = static_cast<uint8_t>(bounds.cone_axis_s8[2]);
            uint8_t c3 = static_cast<uint8_t>(bounds.cone_cutoff_s8);

            meshlet.cone_axis_cutoff = static_cast<uint32_t>(c0) |
                                      (static_cast<uint32_t>(c1) << 8) |
                                      (static_cast<uint32_t>(c2) << 16) |
                                      (static_cast<uint32_t>(c3) << 24);

            meshlet.vertexOffset  = static_cast<uint32_t>(totalAllocatedMeshletVertices + cpuMeshletVertices.size());
            meshlet.indexOffset   = static_cast<uint32_t>(totalAllocatedMeshletTriangles + cpuMeshletTriangles.size());
            meshlet.vertexCount   = m.vertex_count;
            meshlet.triangleCount = m.triangle_count;

            cpuMeshlets.push_back(meshlet);

            for (uint32_t i = 0; i < m.vertex_count; ++i) {
                cpuMeshletVertices.push_back(vertices[m.vertex_offset + i] + subMesh.vertexOffset);
            }
            for (uint32_t i = 0; i < m.triangle_count * 3; ++i) {
                cpuMeshletTriangles.push_back(triangles[m.triangle_offset + i]);
            }
        }

        cpuPositions.insert(cpuPositions.end(), positions.begin(), positions.end());
        cpuAttributes.insert(cpuAttributes.end(), attributes.begin(), attributes.end());
        cpuIndices.insert(cpuIndices.end(), indices.begin(), indices.end());

        return subMesh;
    }

    void Model::uploadToGPU()
    {
        if (cpuPositions.empty() || cpuIndices.empty())
            return;

        vkDeviceWaitIdle(device.getDevice());

        LOG_DEBUG("Model",
            "cpuPositions={}, cpuMeshlets={}, cpuMeshletVerts={}, cpuMeshletTris={}, totalAllocatedVertices={}, totalAllocatedMeshlets={}",
            cpuPositions.size(), cpuMeshlets.size(), cpuMeshletVertices.size(), cpuMeshletTriangles.size(), totalAllocatedVertices, totalAllocatedMeshlets);

        VkDeviceSize newPosSize = cpuPositions.size() * sizeof(VertexPosition);
        VkDeviceSize newAttrSize = cpuAttributes.size() * sizeof(VertexAttribute);
        VkDeviceSize newIdxSize = cpuIndices.size() * sizeof(uint32_t);
        VkDeviceSize newMeshletSize = cpuMeshlets.size() * sizeof(Meshlet);
        VkDeviceSize newMeshletVertSize = cpuMeshletVertices.size() * sizeof(uint32_t);
        VkDeviceSize newMeshletTriSize = cpuMeshletTriangles.size() * sizeof(uint8_t);

        VkDeviceSize oldPosSize = positionBuffer ? positionBuffer->getBufferSize() : 0;
        VkDeviceSize oldAttrSize = attributeBuffer ? attributeBuffer->getBufferSize() : 0;
        VkDeviceSize oldIdxSize = indexBuffer ? indexBuffer->getBufferSize() : 0;
        VkDeviceSize oldMeshletSize = meshletBuffer ? meshletBuffer->getBufferSize() : 0;
        VkDeviceSize oldMeshletVertSize = meshletVerticesBuffer ? meshletVerticesBuffer->getBufferSize() : 0;
        VkDeviceSize oldMeshletTriSize = meshletTrianglesBuffer ? meshletTrianglesBuffer->getBufferSize() : 0;

        auto createStagingBuffer = [&](VkDeviceSize size, const void* data) -> std::unique_ptr<Buffer> {
            if (size == 0) return nullptr;
            auto buf = std::make_unique<Buffer>(device, size, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0);
            buf->writeToBuffer((void*)data, size, 0);
            return buf;
        };

        auto stagingPositions = createStagingBuffer(newPosSize, cpuPositions.data());
        auto stagingAttributes = createStagingBuffer(newAttrSize, cpuAttributes.data());
        auto stagingIndices = createStagingBuffer(newIdxSize, cpuIndices.data());
        auto stagingMeshlets = createStagingBuffer(newMeshletSize, cpuMeshlets.data());
        auto stagingMeshletVerts = createStagingBuffer(newMeshletVertSize, cpuMeshletVertices.data());
        auto stagingMeshletTris = createStagingBuffer(newMeshletTriSize, cpuMeshletTriangles.data());

        auto createExpandedBuffer = [&](VkDeviceSize oldSize, VkDeviceSize newSize, VkBufferUsageFlags usage) -> std::shared_ptr<Buffer> {
            if (oldSize + newSize == 0) return nullptr;
            return std::make_shared<Buffer>(device, oldSize + newSize, 1, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
        };

        auto expandedPosBuffer = createExpandedBuffer(oldPosSize, newPosSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto expandedAttrBuffer = createExpandedBuffer(oldAttrSize, newAttrSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto expandedIdxBuffer = createExpandedBuffer(oldIdxSize, newIdxSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto expandedMeshletBuffer = createExpandedBuffer(oldMeshletSize, newMeshletSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto expandedMeshletVertBuffer = createExpandedBuffer(oldMeshletVertSize, newMeshletVertSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        auto expandedMeshletTriBuffer = createExpandedBuffer(oldMeshletTriSize, newMeshletTriSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        VkCommandBuffer copyCmd = device.beginSingleTimeCommands();

        auto copyOldBuffer = [&](const std::shared_ptr<Buffer>& oldBuf, const std::shared_ptr<Buffer>& newBuf, VkDeviceSize size) {
            if (size > 0) {
                VkBufferCopy cpy {0, 0, size};
                vkCmdCopyBuffer(copyCmd, oldBuf->getBuffer(), newBuf->getBuffer(), 1, &cpy);
            }
        };

        copyOldBuffer(positionBuffer, expandedPosBuffer, oldPosSize);
        copyOldBuffer(attributeBuffer, expandedAttrBuffer, oldAttrSize);
        copyOldBuffer(indexBuffer, expandedIdxBuffer, oldIdxSize);
        copyOldBuffer(meshletBuffer, expandedMeshletBuffer, oldMeshletSize);
        copyOldBuffer(meshletVerticesBuffer, expandedMeshletVertBuffer, oldMeshletVertSize);
        copyOldBuffer(meshletTrianglesBuffer, expandedMeshletTriBuffer, oldMeshletTriSize);

        auto copyNewBuffer = [&](const std::unique_ptr<Buffer>& stagingBuf, const std::shared_ptr<Buffer>& newBuf, VkDeviceSize oldSize, VkDeviceSize newSize) {
            if (newSize > 0) {
                VkBufferCopy cpy {0, oldSize, newSize};
                vkCmdCopyBuffer(copyCmd, stagingBuf->getBuffer(), newBuf->getBuffer(), 1, &cpy);
            }
        };

        copyNewBuffer(stagingPositions, expandedPosBuffer, oldPosSize, newPosSize);
        copyNewBuffer(stagingAttributes, expandedAttrBuffer, oldAttrSize, newAttrSize);
        copyNewBuffer(stagingIndices, expandedIdxBuffer, oldIdxSize, newIdxSize);
        copyNewBuffer(stagingMeshlets, expandedMeshletBuffer, oldMeshletSize, newMeshletSize);
        copyNewBuffer(stagingMeshletVerts, expandedMeshletVertBuffer, oldMeshletVertSize, newMeshletVertSize);
        copyNewBuffer(stagingMeshletTris, expandedMeshletTriBuffer, oldMeshletTriSize, newMeshletTriSize);

        device.endSingleTimeCommands(copyCmd);

        if (oldPosSize > 0 || oldIdxSize > 0) {
            vkDeviceWaitIdle(device.getDevice());
        }

        positionBuffer = std::move(expandedPosBuffer);
        attributeBuffer = std::move(expandedAttrBuffer);
        indexBuffer = std::move(expandedIdxBuffer);
        meshletBuffer = std::move(expandedMeshletBuffer);
        meshletVerticesBuffer = std::move(expandedMeshletVertBuffer);
        meshletTrianglesBuffer = std::move(expandedMeshletTriBuffer);

        totalAllocatedVertices += cpuPositions.size();
        totalAllocatedIndices += cpuIndices.size();
        totalAllocatedMeshlets += cpuMeshlets.size();
        totalAllocatedMeshletVertices += cpuMeshletVertices.size();
        totalAllocatedMeshletTriangles += cpuMeshletTriangles.size();

        cpuPositions.clear(); cpuPositions.shrink_to_fit();
        cpuAttributes.clear(); cpuAttributes.shrink_to_fit();
        cpuIndices.clear(); cpuIndices.shrink_to_fit();
        cpuMeshlets.clear(); cpuMeshlets.shrink_to_fit();
        cpuMeshletVertices.clear(); cpuMeshletVertices.shrink_to_fit();
        cpuMeshletTriangles.clear(); cpuMeshletTriangles.shrink_to_fit();
    }

    void Model::bind(VkCommandBuffer commandBuffer)
    {
        if (!positionBuffer || !indexBuffer)
            return;

        VkBuffer vertexBuffers[] = {positionBuffer->getBuffer(), attributeBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0, 0};

        vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }

    void Model::bindPositionOnly(VkCommandBuffer commandBuffer)
    {
        if (!positionBuffer || !indexBuffer)
            return;

        VkBuffer vertexBuffers[] = {positionBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
} // namespace Engine
