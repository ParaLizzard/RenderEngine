#pragma once

#include "Core/Device.h"
#include "Core/Buffer.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Engine
{
    class Model
    {
    public:
        struct SubMesh
        {
            uint32_t indexCount = 0;
            uint32_t firstIndex = 0;
            int32_t  vertexOffset = 0;
        };

        struct VertexPosition
        {
            glm::vec3 position{};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };

        struct VertexAttribute
        {
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};
            glm::vec4 tangent{};
            uint32_t texId{0};
        };

        Model(Device& device);
        ~Model();

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        SubMesh registerMesh(const std::vector<VertexPosition>& positions,
                             const std::vector<VertexAttribute>& attributes,
                             const std::vector<uint32_t>& indices);

        void uploadToGPU();

        void bind(VkCommandBuffer commandBuffer);

        void bindPositionOnly(VkCommandBuffer commandBuffer);

        std::shared_ptr<Buffer> getPositionBuffer() const { return positionBuffer ? positionBuffer : VK_NULL_HANDLE; }
        std::shared_ptr<Buffer> getAttributeBuffer() const { return attributeBuffer ? attributeBuffer : VK_NULL_HANDLE; }
        std::shared_ptr<Buffer> getIndexBuffer() const { return indexBuffer ? indexBuffer : VK_NULL_HANDLE; }

    private:
        Device& device;

        uint32_t totalAllocatedVertices = 0;
        uint32_t totalAllocatedIndices = 0;

        // CPU staging arrays
        std::vector<VertexPosition> cpuPositions;
        std::vector<VertexAttribute> cpuAttributes;
        std::vector<uint32_t> cpuIndices;

        // Unified GPU-Only
        std::shared_ptr<Buffer> positionBuffer;
        std::shared_ptr<Buffer> attributeBuffer;
        std::shared_ptr<Buffer> indexBuffer;
    };
}