#include "Renderer/Passes/TransformUpdatePassNode.h"
#include "Core/EngineConstants.h"

namespace Engine {

    TransformUpdatePassNode::TransformUpdatePassNode(Device &device, Renderer &renderer):
        RenderPassNode("Transform Update Pass"), device(device), renderer(renderer)
    {
        globalObjectBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        stagingBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            globalObjectBuffers[i] = std::make_shared<Buffer>(
                device,
                sizeof(ObjectData),
                Constants::MAX_SCENE_OBJECTS,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0
            );

            stagingBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(ObjectData),
                Constants::MAX_SCENE_OBJECTS,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0
            );
        }
    }

    void TransformUpdatePassNode::setup(RenderGraphBuilder &renderGraph)
    {

    }

    void TransformUpdatePassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        if (sceneDirty) {
            objectDataArray.clear();

            for (const auto &obj: *frameInfo.gameObjects) {
                ObjectData data {};
                data.modelMatrix = obj.currentWorldMatrix;
                data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(obj.currentWorldMatrix))));
                data.boundingSphere = obj.boundingSphere;
                data.baseMeshlet = obj.subMesh.baseMeshlet;
                data.meshletCount = obj.subMesh.meshletCount;
                data.alphaMode = static_cast<uint32_t>(obj.alphaMode) | (obj.doubleSided ? 4u : 0u);
                data.materialId = obj.subMesh.materialIndex;
                objectDataArray.push_back(data);
            }

            sceneDirty = false;
            framesToUpdate = Constants::MAX_FRAMES_IN_FLIGHT;
        }

        if (framesToUpdate > 0) {
            if (!objectDataArray.empty()) {
                VkDeviceSize bufferSize = objectDataArray.size() * sizeof(ObjectData);
                stagingBuffers[currentFrame]->writeToBuffer(objectDataArray.data(), bufferSize, 0);

                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = 0;
                copyRegion.size = bufferSize;
                vkCmdCopyBuffer(cmd, stagingBuffers[currentFrame]->getBuffer(), globalObjectBuffers[currentFrame]->getBuffer(), 1, &copyRegion);

                VkBufferMemoryBarrier2 barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

                VkPipelineStageFlags2 dstStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                 VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                if (device.isMeshShaderSupported()) {
                    dstStages |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
                }

                barrier.dstStageMask = dstStages;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = globalObjectBuffers[currentFrame]->getBuffer();
                barrier.offset = 0;
                barrier.size = bufferSize;

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.bufferMemoryBarrierCount = 1;
                depInfo.pBufferMemoryBarriers = &barrier;

                vkCmdPipelineBarrier2(cmd, &depInfo);
            }
            framesToUpdate--;
        }
    }

    void TransformUpdatePassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }
} // namespace Engine
