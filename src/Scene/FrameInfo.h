#pragma once

#include "Scene/Camera.h"
#include "Scene/GameObject.h"

namespace Engine
{
    class JobSystem;
    class RenderGraph;

    struct FrameInfo
    {
        int frameIndex;
        float frameTime;
        VkExtent2D extent;
        VkCommandBuffer commandBuffer;
        Camera& camera;
        std::vector<GameObject>& gameObjects;
        const RenderGraph* renderGraph = nullptr;
        JobSystem* jobSystem;
        bool enableSSAO = true;
    };

    struct SceneUbo
    {
        glm::vec4 cameraPosition;
        glm::vec4 directionalLight;
        float maxReflectionLod;
        uint32_t blueNoiseTexIndex;
        glm::vec2 padding;
    };


}
