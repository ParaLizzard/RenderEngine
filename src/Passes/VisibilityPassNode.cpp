#include "VisibilityPassNode.h"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "Renderer/RenderGraph.h"
#include "Renderer/ShaderUtils.h"

namespace Engine
{
    namespace
    {
        void vkCheck(VkResult result, const char* what)
        {
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error(
                    std::string("VisibilityPassNode: ") + what + " failed with VkResult " + std::to_string(result));
            }
        }
    }

    VisibilityPassNode::VisibilityPassNode(Device& device, Renderer& renderer, Model& megaBuffer,
                                           CullPassNode& cullPass) :
        device(device), megaBuffer(megaBuffer), renderer(renderer), cullPass(cullPass)
    {
        createPipelineLayout();
        createPipeline();
    }

    VisibilityPassNode::~VisibilityPassNode()
    {
        if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device.getDevice(), pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device.getDevice(), pipelineLayout, nullptr);
    }

    void VisibilityPassNode::setup(RenderGraphBuilder& graph)
    {
        graph.createTransientImage("VisBuffer", VK_FORMAT_R32G32_UINT, renderer.getSwapChain().getSwapChainExtent(),
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        graph.readBuffer("CullCompactedIndirectCommands", VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        graph.readBuffer("CullDrawCount", VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

        graph.writeImage("VisBuffer", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }

    void VisibilityPassNode::resolve(RenderGraph& graph, const FrameInfo& frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }

    void VisibilityPassNode::execute(VkCommandBuffer& cmd, FrameInfo& info)
    {
        const uint32_t activeObjectCount = cullPass.getActiveObjectCount();
        if (activeObjectCount == 0) return;

        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = info.renderGraph->getImageView("VisBuffer");
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.uint32[0] = 0xFFFFFFFFu;
        colorAttachment.clearValue.color.uint32[1] = 0xFFFFFFFFu;

        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        renderingInfo.renderArea = {{0, 0}, info.extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkViewport vp{
            0.0f, 0.0f, static_cast<float>(info.extent.width), static_cast<float>(info.extent.height), 0.0f, 1.0f
        };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{{0, 0}, info.extent};
        vkCmdSetScissor(cmd, 0, 1, &sc);

        VkDescriptorSet set = cullPass.getObjectDescriptorSet(info.frameIndex);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 0, nullptr);

        std::shared_ptr<Buffer> vertexBuffers = {megaBuffer.getPositionBuffer()};
        VkDeviceSize vertexOffsets[] = {0};
        VkBuffer buffer = vertexBuffers->getBuffer();
        vkCmdBindVertexBuffers(cmd, 0, 1, &(buffer), vertexOffsets);
        vkCmdBindIndexBuffer(cmd, megaBuffer.getIndexBuffer()->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        ComputePushConstants pc{};
        pc.viewProjection = info.camera->getProjection() * info.camera->getView();
        pc.objectCount = activeObjectCount;
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

        vkCmdDrawIndexedIndirectCount(cmd,

                                      cullPass.getCompactedIndirectBuffer(info.frameIndex), 0,
                                      cullPass.getDrawCountBuffer(info.frameIndex), 0,
                                      activeObjectCount, sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmd);
    }

    void VisibilityPassNode::createPipelineLayout()
    {
        VkDescriptorSetLayout layout = cullPass.getObjectSetLayout();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &layout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        vkCheck(vkCreatePipelineLayout(device.getDevice(), &layoutInfo, nullptr, &pipelineLayout),
                "vkCreatePipelineLayout");
    }

    void VisibilityPassNode::createPipeline()
    {
        auto vertCode = ShaderUtils::readFile("shaders/visbuffer.vert.spv");
        auto fragCode = ShaderUtils::readFile("shaders/visbuffer.frag.spv");
        VkShaderModule vMod = ShaderUtils::createShaderModule(device.getDevice(), vertCode);
        VkShaderModule fMod = ShaderUtils::createShaderModule(device.getDevice(), fragCode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vMod;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fMod;
        stages[1].pName = "main";

        auto bDesc = Model::VertexPosition::getBindingDescriptions();
        auto fullADesc = Model::VertexPosition::getAttributeDescriptions();

        std::vector<VkVertexInputAttributeDescription> aDesc;
        aDesc.reserve(fullADesc.size());
        for (const auto& attr : fullADesc)
        {
            if (attr.location == 0) aDesc.push_back(attr);
        }

        VkPipelineVertexInputStateCreateInfo vInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bDesc.size());
        vInput.pVertexBindingDescriptions = bDesc.data();
        vInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(aDesc.size());
        vInput.pVertexAttributeDescriptions = aDesc.data();

        VkPipelineInputAssemblyStateCreateInfo iAsm{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        iAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rast{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rast.polygonMode = VK_POLYGON_MODE_FILL;
        rast.cullMode = VK_CULL_MODE_NONE;
        rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rast.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo msaa{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blendState.attachmentCount = 1;
        blendState.pAttachments = &blendAttachment;

        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat fmt = VK_FORMAT_R32G32_UINT;
        VkPipelineRenderingCreateInfo rInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rInfo.colorAttachmentCount = 1;
        rInfo.pColorAttachmentFormats = &fmt;

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.pNext = &rInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vInput;
        pipelineInfo.pInputAssemblyState = &iAsm;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rast;
        pipelineInfo.pMultisampleState = &msaa;
        pipelineInfo.pColorBlendState = &blendState;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;

        VkResult result = vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo,
                                                    nullptr, &pipeline);
        vkDestroyShaderModule(device.getDevice(), vMod, nullptr);
        vkDestroyShaderModule(device.getDevice(), fMod, nullptr);
        vkCheck(result, "vkCreateGraphicsPipelines");
    }
}
