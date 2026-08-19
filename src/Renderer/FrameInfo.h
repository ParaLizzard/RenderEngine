#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Scene/Camera.h"
#include "Scene/GameObject.h"
#include "System/Input/InputManager.h"

#define SHADOW_ATLAS_WIDTH 2048
#define SHADOW_ATLAS_HEIGHT 3072
#define SHADOW_MAP_CASCADES 3
#define SHADOW_CASCADE0_SIZE 2048
#define SHADOW_CASCADE1_SIZE 1024
#define SHADOW_CASCADE2_SIZE 1024

namespace Engine {
    class JobSystem;
    class RenderGraph;
    class ResourceHeap;
    class Renderer;
    class Device;
    class Model;



    struct FrameInfo
    {
        int frameIndex;
        float frameTime;
        VkExtent2D extent;
        VkCommandBuffer commandBuffer;
        Camera *camera;
        InputManager *input;
        std::vector<GameObject> *gameObjects;

        Device *device = nullptr;
        RenderGraph *renderGraph = nullptr;
        Renderer *renderer = nullptr;
        Model *megaBuffer = nullptr;
        ResourceHeap *resourceHeap = nullptr;
        const JobSystem *jobSystem;

        bool enableSSAO = true;

        glm::mat4 cullViewProj;
        glm::vec3 cullCameraPos;
        glm::mat4 cullView{1.0f};
        bool cullEnabled = true;
        bool firstFrame = false;
        glm::vec2 subpixelJitter{0.0f, 0.0f};
        glm::mat4 curViewProj{1.0f};
        int debugViewMode = 0;
        int debugHiZMipLevel = 0;
    };

    struct SceneUbo
    {
        glm::mat4 viewProjection;
        glm::mat4 prevViewProjection;
        glm::vec4 frustumPlanes[6];
        glm::vec4 cameraPosition;
        glm::vec4 directionalLight;
        glm::mat4 lightViewProj[SHADOW_MAP_CASCADES];
        glm::vec4 cascadesSplits;
        float maxReflectionLod;
        uint32_t blueNoiseTexIndex;
        glm::vec2 padding;
    };


} // namespace Engine



class PerformanceMonitor
{
private:
    float timer = 0.0f;
    int count = 0;
    float interval = 2.0f;

    float cachedFPS = 0.0f;
    float cachedFrameTimeMs = 0.0f;
public:
    void tick(float dt)
    {
        timer += dt;
        count++;

        if (timer >= interval) {
            cachedFPS = static_cast<float>(count) / timer;
            cachedFrameTimeMs = (timer * 1000.0f) / static_cast<float>(count);

            timer -= interval;
            count = 0;
        }
    }

    float GetAverageFPS()
    {
        return cachedFPS;
    }
    float GetAverageFrameTime()
    {
        return cachedFrameTimeMs;
    }
};
