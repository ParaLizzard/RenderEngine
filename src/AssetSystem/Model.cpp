#include "AssetSystem/Model.h"
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
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(6);

        // Location 0: Position (Binding 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(VertexPosition, position);

        // Location 1: Color (Binding 1)
        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(VertexAttribute, color);

        // Location 2: Normal (Binding 1)
        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(VertexAttribute, normal);

        // Location 3: UV (Binding 1)
        attributeDescriptions[3].binding = 1;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(VertexAttribute, uv);

        // Location 4: Tangent (Binding 1)
        attributeDescriptions[4].binding = 1;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(VertexAttribute, tangent);

        // Location 5: texture ID (Binding 1)
        attributeDescriptions[5].binding = 1;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32_UINT;
        attributeDescriptions[5].offset = offsetof(VertexAttribute, texId);

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

        cpuPositions.insert(cpuPositions.end(), positions.begin(), positions.end());
        cpuAttributes.insert(cpuAttributes.end(), attributes.begin(), attributes.end());
        cpuIndices.insert(cpuIndices.end(), indices.begin(), indices.end());

        return subMesh;
    }

    void Model::uploadToGPU()
    {
        if (cpuPositions.empty() || cpuIndices.empty())
            return;

        VkDeviceSize newPosSize = cpuPositions.size() * sizeof(VertexPosition);
        VkDeviceSize newAttrSize = cpuAttributes.size() * sizeof(VertexAttribute);
        VkDeviceSize newIdxSize = cpuIndices.size() * sizeof(uint32_t);

        VkDeviceSize oldPosSize = positionBuffer ? positionBuffer->getBufferSize() : 0;
        VkDeviceSize oldAttrSize = attributeBuffer ? attributeBuffer->getBufferSize() : 0;
        VkDeviceSize oldIdxSize = indexBuffer ? indexBuffer->getBufferSize() : 0;

        Buffer stagingPositions(
            device, newPosSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0);
        Buffer stagingAttributes(
            device, newAttrSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0);
        Buffer stagingIndices(device, newIdxSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0);

        stagingPositions.writeToBuffer(cpuPositions.data(), newPosSize, 0);
        stagingAttributes.writeToBuffer(cpuAttributes.data(), newAttrSize, 0);
        stagingIndices.writeToBuffer(cpuIndices.data(), newIdxSize, 0);

        auto expandedPosBuffer =
            std::make_shared<Buffer>(device,
                                     oldPosSize + newPosSize,
                                     1,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     0);

        auto expandedAttrBuffer =
            std::make_shared<Buffer>(device,
                                     oldAttrSize + newAttrSize,
                                     1,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     0);

        auto expandedIdxBuffer =
            std::make_shared<Buffer>(device,
                                     oldIdxSize + newIdxSize,
                                     1,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     0);

        VkCommandBuffer copyCmd = device.beginSingleTimeCommands();

        if (oldPosSize > 0) {
            VkBufferCopy cpyPos {0, 0, oldPosSize};
            vkCmdCopyBuffer(copyCmd, positionBuffer->getBuffer(), expandedPosBuffer->getBuffer(), 1, &cpyPos);

            VkBufferCopy cpyAttr {0, 0, oldAttrSize};
            vkCmdCopyBuffer(copyCmd, attributeBuffer->getBuffer(), expandedAttrBuffer->getBuffer(), 1, &cpyAttr);
        }
        if (oldIdxSize > 0) {
            VkBufferCopy cpyIdx {0, 0, oldIdxSize};
            vkCmdCopyBuffer(copyCmd, indexBuffer->getBuffer(), expandedIdxBuffer->getBuffer(), 1, &cpyIdx);
        }

        VkBufferCopy posCpy {0, oldPosSize, newPosSize};
        vkCmdCopyBuffer(copyCmd, stagingPositions.getBuffer(), expandedPosBuffer->getBuffer(), 1, &posCpy);

        VkBufferCopy attrCpy {0, oldAttrSize, newAttrSize};
        vkCmdCopyBuffer(copyCmd, stagingAttributes.getBuffer(), expandedAttrBuffer->getBuffer(), 1, &attrCpy);

        VkBufferCopy idxCpy {0, oldIdxSize, newIdxSize};
        vkCmdCopyBuffer(copyCmd, stagingIndices.getBuffer(), expandedIdxBuffer->getBuffer(), 1, &idxCpy);

        device.endSingleTimeCommands(copyCmd);

        positionBuffer = std::move(expandedPosBuffer);
        attributeBuffer = std::move(expandedAttrBuffer);
        indexBuffer = std::move(expandedIdxBuffer);

        totalAllocatedVertices += cpuPositions.size();
        totalAllocatedIndices += cpuIndices.size();

        cpuPositions.clear();
        cpuPositions.shrink_to_fit();
        cpuAttributes.clear();
        cpuAttributes.shrink_to_fit();
        cpuIndices.clear();
        cpuIndices.shrink_to_fit();
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
