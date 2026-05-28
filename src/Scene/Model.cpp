#include "Model.h"

namespace Engine
{
    Model::Model(Device& device, uint32_t chunkVertexCapacity, uint32_t chunkIndexCapacity):device(device), chunkVertexCapacity(chunkVertexCapacity), chunkIndexCapacity(chunkIndexCapacity)
    {
        createNewChunk();
    }

    Model::~Model()
    {
        vkDeviceWaitIdle(device.getDevice());
    }

    std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);

        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(6);

        // Location 0: Position (vec3)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Location 1: Color (vec3)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Location 2: Normal (vec3)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        // Location 3: UV (vec2)
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, uv);

        // Location 4: Tangent (vec4)
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, tangent);

        // Location 5: texture ID (uint)
        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32_UINT;
        attributeDescriptions[5].offset = offsetof(Vertex, texId);

        return attributeDescriptions;
    }

    SubMesh Model::registerMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        uint32_t verticesCount = vertices.size();
        uint32_t indexCount = indices.size();

        assert(verticesCount <= chunkVertexCapacity && indexCount <= chunkIndexCapacity && "Mesh is too big");

        if (activeChunkVertexCount + verticesCount > chunkVertexCapacity || activeChunkIndexCount + indexCount > chunkIndexCapacity)
        {
            createNewChunk();
        }

        uint32_t vertexByteOffset = activeChunkVertexCount * sizeof(Vertex);
        uint32_t indexByteOffset = activeChunkIndexCount * sizeof(uint32_t);

        vertexBuffers.back()->writeToBuffer(vertices.data(), verticesCount*sizeof(Vertex), vertexByteOffset);
        indexBuffers.back()->writeToBuffer(indices.data(), indexCount * sizeof(uint32_t), indexByteOffset);

        SubMesh subMesh{};
        subMesh.bufferIndex = vertexBuffers.size() - 1;
        subMesh.indexCount = indexCount;
        subMesh.firstIndex = activeChunkIndexCount;
        subMesh.vertexOffset = (int32_t)activeChunkVertexCount;

        activeChunkVertexCount += verticesCount;
        activeChunkIndexCount += indexCount;

        return subMesh;
    }

    void Model::bind(VkCommandBuffer commandBuffer, uint32_t chunkIndex)
    {
        VkBuffer vertexBuffer = vertexBuffers[chunkIndex]->getBuffer();
        VkBuffer indexBuffer = indexBuffers[chunkIndex]->getBuffer();

        VkDeviceSize offset = 0;

        vkCmdBindVertexBuffers(commandBuffer, 0, 1,&vertexBuffer,&offset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void Model::createNewChunk()
    {
        std::unique_ptr<Buffer> vertexBuffer = std::make_unique<Buffer>(device, (uint32_t)sizeof(Vertex), chunkVertexCapacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, 0);
        vertexBuffers.push_back(std::move(vertexBuffer));
        std::unique_ptr<Buffer> indexBuffer = std::make_unique<Buffer>(device, (uint32_t)sizeof(uint32_t), chunkIndexCapacity, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, 0);
        indexBuffers.push_back(std::move(indexBuffer));

        activeChunkVertexCount = 0;
        activeChunkIndexCount = 0;
    }
}
