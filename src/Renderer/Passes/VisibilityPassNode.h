#pragma once
#include "Renderer/RenderPassNode.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace Engine {
    class CullPassNode;
    struct VisibilityPushConstants
    {
        glm::mat4 viewProjection;
        glm::vec4 frustumPlanes[6];
        glm::vec3 cameraPos;
        uint32_t cullFlags;
        uint32_t objectCount;
        uint32_t actualObjectCount;
        float projM11;
        uint32_t objectCapacity;
        uint32_t clipPlaneCount;
        uint32_t isMeshShader;
    };

    class VisibilityPassNode: public RenderPassNode
    {
    public:
        VisibilityPassNode(Device &device, Renderer &renderer, Model &megaBuffer, CullPassNode &cullPass, ResourceHeap &resourceHeap);
        ~VisibilityPassNode();

        VisibilityPassNode(const VisibilityPassNode &) = delete;
        VisibilityPassNode &operator=(const VisibilityPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;


    private:
        void createPipelineLayout();
        void createPipeline();
        void createMeshPipeline();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        CullPassNode &cullPass;
        ResourceHeap &resourceHeap;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkPipeline meshPipeline {VK_NULL_HANDLE};
        VkPipelineLayout meshPipelineLayout {VK_NULL_HANDLE};
        PFN_vkCmdDrawMeshTasksIndirectEXT pfn_vkCmdDrawMeshTasksIndirectEXT = nullptr;
        PFN_vkCmdDrawMeshTasksEXT pfn_vkCmdDrawMeshTasksEXT = nullptr;


    };
} // namespace Engine
