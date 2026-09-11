#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "Scene/Camera.h"
#include "Scene/GameObject.h"
#include "System/Input/InputSubsystem.h"

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
    class VulkanDevice;
    class Model;



    struct FrameInfo
    {
        int frameIndex;
        float frameTime;
        VkExtent2D extent;
        VkCommandBuffer commandBuffer;
        Camera *camera;
        InputSubsystem *input = nullptr;
        std::vector<GameObject> *gameObjects;

        VulkanDevice *device = nullptr;
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
