#pragma once

#include "Window.h"
#include "Device.h"
#include "Scene/Camera.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/Texture.h"
#include <format>
#include <unordered_map>
#include <limits>
#include "Core/JobSystem.h"
#include "Scene/KeyboardMovement.h"

namespace Engine
{
    class Application
    {
    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void run();
    private:
        static unsigned int computeWorkerThreadCount();

        Window window{WIDTH,HEIGHT,"Render Engine"};
        Device device{window};
        Renderer renderer{window,device};
        Model megaBuffer{device};
        ResourceHeap resourceHeap{device};
        RenderGraph renderGraph{device};
        KeyboardMovementController cameraController{};
        std::shared_ptr<GameObject> cameraObject;
        JobSystem jobSystem{ computeWorkerThreadCount() };

        bool enableSSAO = true;
        bool ssaoKeyPressed = false;

        std::deque<Texture2D> sceneTextures;
        std::vector<GameObject> gameObjects;

        float fpsTimer = 0.0f;
        int fpsCount = 0;

    };
}