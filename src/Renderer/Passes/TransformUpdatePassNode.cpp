#include "Renderer/Passes/TransformUpdatePassNode.h"
#include "Core/EngineConfig.h"

namespace Engine {

    TransformUpdatePassNode::TransformUpdatePassNode(Device &device, Renderer &renderer):
        device(device), renderer(renderer)
    {
        // Allocate a single static GPU-only buffer for all objects
        globalObjectBuffer = std::make_shared<Buffer>(
            device,
            sizeof(ObjectData),
            Config::MAX_SCENE_OBJECTS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0
        );

        stagingBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            stagingBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(ObjectData),
                Config::MAX_SCENE_OBJECTS,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_CPU_ONLY,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0
            );
        }
    }

    void TransformUpdatePassNode::setup(RenderGraphBuilder &renderGraph)
    {
        // We do not register the globalObjectBuffer into the render graph 
        // because it is globally bound via ResourceHeap.
        // However, we could register a dummy resource to force a dependency if needed.
    }

    void TransformUpdatePassNode::execute(VkCommandBuffer &cmd, FrameInfo &frameInfo)
    {
        uint32_t currentFrame = renderer.getFrameIndex();

        if (sceneDirty) {
            objectDataArray.clear();

            for (const auto &obj: *frameInfo.gameObjects) {
                if (obj.subMesh.indexCount == 0)
                    continue;
                if (obj.alphaMode == AlphaMode::Blend)
                    continue;

                ObjectData data {};
                data.modelMatrix = obj.currentWorldMatrix;
                data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(obj.currentWorldMatrix))));
                data.boundingSphere = obj.boundingSphere;
                data.baseMeshlet = obj.subMesh.baseMeshlet;
                data.meshletCount = obj.subMesh.meshletCount;
                objectDataArray.push_back(data);
            }

            sceneDirty = false;
            framesToUpdate = Config::MAX_FRAMES_IN_FLIGHT;
        }

        if (framesToUpdate > 0) {
            if (!objectDataArray.empty()) {
                VkDeviceSize bufferSize = objectDataArray.size() * sizeof(ObjectData);
                stagingBuffers[currentFrame]->writeToBuffer(objectDataArray.data(), bufferSize, 0);

                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = 0;
                copyRegion.size = bufferSize;
                vkCmdCopyBuffer(cmd, stagingBuffers[currentFrame]->getBuffer(), globalObjectBuffer->getBuffer(), 1, &copyRegion);
                
                // Add a barrier so compute culling waits for the transfer to finish
                VkBufferMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = globalObjectBuffer->getBuffer();
                barrier.offset = 0;
                barrier.size = bufferSize;

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    1, &barrier,
                    0, nullptr
                );
            }
            framesToUpdate--;
        }
    }

    void TransformUpdatePassNode::resolve(RenderGraph &graph, const FrameInfo &frameInfo)
    {
        RenderPassNode::resolve(graph, frameInfo);
    }
} // namespace Engine
