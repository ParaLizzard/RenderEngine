#pragma once

#include "Scene/Camera.h"
#include "Scene/GameObject.h"

namespace Engine
{

    struct FrameInfo
    {
        int frameIndex;
        float frameTime;
        VkExtent2D extent;
        VkCommandBuffer commandBuffer;
        Camera& camera;
        std::vector<GameObject>& gameObjects;
    };
}
