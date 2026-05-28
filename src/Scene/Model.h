#pragma once

#include "Core/Device.h"
#include "Core/Buffer.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Engine
{

    struct SubMesh
    {
        uint32_t bufferIndex;
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
    };

    class Model
    {
    public:
        struct Vertex
        {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};
            glm::vec4 tangent{};
            uint32_t texId{0};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            bool operator==(const Vertex& other) const {
                return position == other.position && color == other.color && normal == other.normal &&
                       uv == other.uv && tangent == other.tangent && texId == other.texId;
            }
        };

        Model(Device& device, uint32_t chunkVertexCapacity = 500000, uint32_t chunkIndexCapacity = 1000000);
        ~Model();

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        SubMesh registerMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);


        void bind(VkCommandBuffer commandBuffer, uint32_t chunkIndex);

    private:

        void createNewChunk();

        Device& device;

        std::vector<std::unique_ptr<Buffer>> vertexBuffers;
        std::vector<std::unique_ptr<Buffer>> indexBuffers;

        uint32_t chunkVertexCapacity;
        uint32_t chunkIndexCapacity;

        uint32_t activeChunkVertexCount = 0;
        uint32_t activeChunkIndexCount = 0;
    };
}