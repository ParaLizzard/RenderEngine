#pragma once

#include <limits>
#include <unordered_map>
#include <format>
#include "Core/JobSystem.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/Camera.h"
#include "Scene/KeyboardMovement.h"
#include "Scene/Texture.h"
#include "Device.h"
#include "Window.h"

namespace Engine {
    class Application
    {
    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        Application();
        ~Application();

        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        void run();

    private:
        Window window {WIDTH, HEIGHT, "Render Engine"};
        Device device {window};
        Renderer renderer {window, device};
        Model megaBuffer {device};
        ResourceHeap resourceHeap {device};
        RenderGraph renderGraph {device};
        KeyboardMovementController cameraController {};
        std::shared_ptr<GameObject> cameraObject;
        JobSystem jobSystem {std::max(1u, std::thread::hardware_concurrency() - 1)};

        bool enableSSAO = true;
        bool ssaoKeyPressed = false;

        std::deque<Texture2D> sceneTextures;
        std::vector<GameObject> gameObjects;

        float fpsTimer = 0.0f;
        int fpsCount = 0;
    };
} // namespace Engine
