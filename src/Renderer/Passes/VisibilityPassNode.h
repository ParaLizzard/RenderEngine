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
        glm::mat4 view;
        glm::vec4 projParams;
        glm::vec4 hizParams;
        glm::vec2 screenParams;
        uint32_t cullFlags;
        uint32_t objectCount;
        uint32_t actualObjectCount;
        uint32_t objectCapacity;
        uint32_t clipPlaneCount;
        uint32_t isMeshShader;
        uint32_t phase;
        uint32_t pad;
    };

    class VisibilityPassNode: public RenderPassNode
    {
    public:
        VisibilityPassNode(Device &device, Renderer &renderer, Model &megaBuffer, CullPassNode &cullPass, ResourceHeap &resourceHeap, uint32_t phase = 0);
        ~VisibilityPassNode();

        VisibilityPassNode(const VisibilityPassNode &) = delete;
        VisibilityPassNode &operator=(const VisibilityPassNode &) = delete;

        void setup(RenderGraphBuilder &renderGraph) override;
        void registerResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void updateResources(RenderGraph &graph, const FrameInfo &frameInfo) override;
        void execute(VkCommandBuffer &cmd, FrameInfo &frameInfo) override;
        void resolve(RenderGraph &graph, const FrameInfo &frameInfo) override;

        [[nodiscard]] uint32_t getPhase() const { return phase; }

    private:
        void createPipelineLayout();
        void createPipeline();
        void createMeshPipeline();
        void createMaskedPipeline();

        Device &device;
        Renderer &renderer;
        Model &megaBuffer;
        CullPassNode &cullPass;
        ResourceHeap &resourceHeap;
        uint32_t phase = 0;

        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkPipeline meshPipeline {VK_NULL_HANDLE};
        VkPipelineLayout meshPipelineLayout {VK_NULL_HANDLE};
        VkPipelineLayout maskedPipelineLayout {VK_NULL_HANDLE};
        VkPipeline       maskedPipeline       {VK_NULL_HANDLE};

        PFN_vkCmdDrawMeshTasksIndirectEXT pfn_vkCmdDrawMeshTasksIndirectEXT = nullptr;
        PFN_vkCmdDrawMeshTasksEXT pfn_vkCmdDrawMeshTasksEXT = nullptr;
    };
}
