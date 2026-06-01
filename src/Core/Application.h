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

        void run();
    private:
        Window window{WIDTH,HEIGHT,"Render Engine"};
        Device device{window};
        ResourceHeap resourceHeap{device};
        RenderGraph renderGraph{device};
        Renderer renderer{window,device};
        Model megaBuffer{device, 500000, 1000000};
        KeyboardMovementController cameraController{};
        std::shared_ptr<GameObject> cameraObject;
        JobSystem jobSystem{ std::max(1u, std::thread::hardware_concurrency() - 1) };

        //std::vector<Texture2D> sceneTextures;
        std::deque<Texture2D> sceneTextures;
        std::vector<GameObject> gameObjects;
    };
}
