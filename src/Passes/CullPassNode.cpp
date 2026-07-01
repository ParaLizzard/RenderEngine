#include "CullPassNode.h"

#include <array>
#include <stdexcept>
#include <string>

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
                throw std::runtime_error(std::string("CullPassNode: ") + what + " failed with VkResult " + std::to_string(result));
            }
        }
    }

    CullPassNode::CullPassNode(Device& device, Renderer& renderer, Model& megaBuffer) :
        device(device), megaBuffer(megaBuffer), renderer(renderer)
    {
        objectDescriptorSets.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

        std::array<VkDescriptorSetLayoutBinding, 4> ssboBindings{};
        for (uint32_t i = 0; i < ssboBindings.size(); ++i)
        {
            ssboBindings[i].binding = i;
            ssboBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ssboBindings[i].descriptorCount = 1;
            ssboBindings[i].stageFlags = (i == 0) ? (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT) : VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<uint32_t>(ssboBindings.size());
        layoutInfo.pBindings = ssboBindings.data();
        vkCheck(vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo, nullptr, &objectSetLayout), "vkCreateDescriptorSetLayout");

        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT * static_cast<uint32_t>(ssboBindings.size())};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = Renderer::MAX_FRAMES_IN_FLIGHT;
        vkCheck(vkCreateDescriptorPool(device.getDevice(), &poolInfo, nullptr, &objectDescriptorPool), "vkCreateDescriptorPool");

        createPerFrameBuffers();
        createDescriptorResources();
        createPipeline();
    }

    CullPassNode::~CullPassNode()
    {
        vkDestroyDescriptorPool(device.getDevice(), objectDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device.getDevice(), objectSetLayout, nullptr);
        vkDestroyPipeline(device.getDevice(), computePipeline, nullptr);
        vkDestroyPipelineLayout(device.getDevice(), computePipelineLayout, nullptr);
    }

    void CullPassNode::createPerFrameBuffers()
    {
        for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            cpuObjectSSBOs.push_back(std::make_unique<Buffer>(device, sizeof(ObjectData), MAX_OBJECTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0));
            gpuObjectSSBOs.push_back(std::make_unique<Buffer>(device, sizeof(ObjectData), MAX_OBJECTS, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0));

            cpuIndirectCommandBuffers.push_back(std::make_unique<Buffer>(device, sizeof(VkDrawIndexedIndirectCommand), MAX_OBJECTS, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0));
            gpuSourceCommandBuffers.push_back(std::make_unique<Buffer>(device, sizeof(VkDrawIndexedIndirectCommand), MAX_OBJECTS, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0));

            gpuCompactedIndirectCommandBuffers.push_back(std::make_unique<Buffer>(device, sizeof(VkDrawIndexedIndirectCommand), MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0));
            gpuDrawCountBuffers.push_back(std::make_unique<Buffer>(device, sizeof(uint32_t), 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, 0));
        }
    }

    void CullPassNode::createDescriptorResources()
    {
        for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.descriptorPool = objectDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &objectSetLayout;
            vkCheck(vkAllocateDescriptorSets(device.getDevice(), &allocInfo, &objectDescriptorSets[i]), "vkAllocateDescriptorSets");

            VkDescriptorBufferInfo objInfo = gpuObjectSSBOs[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo compInfo = gpuCompactedIndirectCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo countInfo = gpuDrawCountBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);
            VkDescriptorBufferInfo srcCmdInfo = gpuSourceCommandBuffers[i]->descriptorInfo(VK_WHOLE_SIZE, 0);

            std::array<VkWriteDescriptorSet, 4> writes{};
            for (uint32_t b = 0; b < writes.size(); ++b)
            {
                writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[b].dstSet = objectDescriptorSets[i];
                writes[b].dstBinding = b;
                writes[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writes[b].descriptorCount = 1;
            }
            writes[0].pBufferInfo = &objInfo;
            writes[1].pBufferInfo = &compInfo;
            writes[2].pBufferInfo = &countInfo;
            writes[3].pBufferInfo = &srcCmdInfo;
            vkUpdateDescriptorSets(device.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    void CullPassNode::createPipeline()
    {
        auto compCode = ShaderUtils::readFile("shaders/cull.comp.spv");
        VkShaderModule mod = ShaderUtils::createShaderModule(device.getDevice(), compCode);

        VkPushConstantRange range{};
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset = 0;
        range.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &objectSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &range;
        vkCheck(vkCreatePipelineLayout(device.getDevice(), &layoutInfo, nullptr, &computePipelineLayout), "vkCreatePipelineLayout");

        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = mod;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = computePipelineLayout;

        VkResult result = vkCreateComputePipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr, &computePipeline);
        vkDestroyShaderModule(device.getDevice(), mod, nullptr);
        vkCheck(result, "vkCreateComputePipelines");
    }

    void CullPassNode::setup(RenderGraphBuilder& graph)
    {
        graph.writeBuffer("CullObjectData", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        graph.writeBuffer("CullCompactedIndirectCommands", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        graph.writeBuffer("CullDrawCount", VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    void CullPassNode::rebuildSceneArrays(const FrameInfo& info)
    {
        objectDataArray.clear();
        indirectCommandsArray.clear();

        if (!info.gameObjects) return;

        objectDataArray.reserve(info.gameObjects->size());
        indirectCommandsArray.reserve(info.gameObjects->size());

        for (const auto& obj : *info.gameObjects)
        {
            if (obj.subMesh.indexCount == 0) continue;
            if (objectDataArray.size() >= MAX_OBJECTS) break;

            ObjectData data{};
            data.modelMatrix = obj.currentWorldMatrix;
            data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(obj.currentWorldMatrix))));
            data.boundingSphere = obj.boundingSphere;
            objectDataArray.push_back(data);

            VkDrawIndexedIndirectCommand cmd{};
            cmd.indexCount = obj.subMesh.indexCount;
            cmd.instanceCount = 1;
            cmd.firstIndex = obj.subMesh.firstIndex;
            cmd.vertexOffset = obj.subMesh.vertexOffset;
            cmd.firstInstance = 0;
            indirectCommandsArray.push_back(cmd);
        }

        sceneDirty = false;
    }

    void CullPassNode::uploadFrameData(uint32_t frame, VkCommandBuffer cmd)
    {
        const VkDeviceSize objectBytes = objectDataArray.size() * sizeof(ObjectData);
        const VkDeviceSize commandBytes = indirectCommandsArray.size() * sizeof(VkDrawIndexedIndirectCommand);

        cpuObjectSSBOs[frame]->writeToBuffer(objectDataArray.data(), objectBytes, 0);
        cpuIndirectCommandBuffers[frame]->writeToBuffer(indirectCommandsArray.data(), commandBytes, 0);

        VkBufferCopy objCopy{};
        objCopy.srcOffset = 0;
        objCopy.dstOffset = 0;
        objCopy.size = objectBytes;
        vkCmdCopyBuffer(cmd, cpuObjectSSBOs[frame]->getBuffer(), gpuObjectSSBOs[frame]->getBuffer(), 1, &objCopy);

        VkBufferCopy cmdCopy{};
        cmdCopy.srcOffset = 0;
        cmdCopy.dstOffset = 0;
        cmdCopy.size = commandBytes;
        vkCmdCopyBuffer(cmd, cpuIndirectCommandBuffers[frame]->getBuffer(), gpuSourceCommandBuffers[frame]->getBuffer(), 1, &cmdCopy);

        vkCmdFillBuffer(cmd, gpuDrawCountBuffers[frame]->getBuffer(), 0, sizeof(uint32_t), 0);

        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

        VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.memoryBarrierCount = 1;
        depInfo.pMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    void CullPassNode::dispatchCull(uint32_t frame, VkCommandBuffer cmd, const FrameInfo& info)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &objectDescriptorSets[frame], 0, nullptr);

        ComputePushConstants pc{};
        pc.viewProjection = info.camera->getProjection() * info.camera->getView();
        pc.objectCount = static_cast<uint32_t>(objectDataArray.size());
        vkCmdPushConstants(cmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

        const uint32_t groupCount = (pc.objectCount + 255) / 256;
        vkCmdDispatch(cmd, groupCount, 1, 1);
    }

    void CullPassNode::execute(VkCommandBuffer& cmd, FrameInfo& info)
    {
        const uint32_t frame = renderer.getFrameIndex();

        if (sceneDirty)
        {
            rebuildSceneArrays(info);
        }

        if (objectDataArray.empty())
        {
            dataUploadedThisFrame = false;
            return;
        }

        uploadFrameData(frame, cmd);
        dispatchCull(frame, cmd, info);
        dataUploadedThisFrame = true;
    }

    void CullPassNode::resolve(RenderGraph& graph, const FrameInfo& info) {}
}