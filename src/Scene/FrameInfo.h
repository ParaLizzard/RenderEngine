#pragma once

#include "Scene/Camera.h"
#include "Scene/GameObject.h"



namespace Engine
{
    class JobSystem;
    class RenderGraph;
    class ResourceHeap;
    class Renderer;

    struct FrameInfo
    {
        int frameIndex;
        float frameTime;
        VkExtent2D extent;
        VkCommandBuffer commandBuffer;
        Camera* camera;
        std::vector<GameObject>* gameObjects;

        Device* device = nullptr;
        RenderGraph* renderGraph = nullptr;
        Renderer* renderer = nullptr;
        Model* megaBuffer = nullptr;
        ResourceHeap* resourceHeap = nullptr;
        const JobSystem* jobSystem;

        glm::vec3 cameraPos;

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
