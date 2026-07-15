#pragma once

#include <limits>
#include <unordered_map>
#include <format>

#include "AssetSystem/AssetStreamer.h"
#include "Scene/SceneManager.h"
#include "Threading/JobSystem.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include "Scene/Camera.h"
#include "Scene/GameObject.h"
#include "Scene/KeyboardMovement.h"
#include "AssetSystem/Texture.h"
#include "Vulkan/Device.h"
#include "System/Window/Window.h"
#include "System/Input/InputManager.h"
#include "System/Input/InputBackendWindows.h"
#include "AssetSystem/IBL.h"
#include "Renderer/Passes/CullPassNode.h"
#include "Renderer/Passes/FxaaPassNode.h"
#include "Renderer/Passes/MaterialPassNode.h"
#include "Renderer/Passes/SsaoPassNode.h"
#include "Renderer/Passes/VisibilityPassNode.h"

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
        void initScene();
        void compileFrameGraph();
        void updateFrameGraph();
        void updateSceneGraph();

        Window window {WIDTH, HEIGHT, "Render Engine"};
        Device device {window};
        Renderer renderer {window, device};
        Model megaBuffer {device};
        ResourceHeap resourceHeap {device};
        RenderGraph renderGraph {device};
        KeyboardMovementController cameraController {};
        std::shared_ptr<GameObject> cameraObject;
        JobSystem jobSystem {std::max(1u, std::thread::hardware_concurrency() - 1)};
        SceneManager sceneManager{};
        AssetStreamer assetStreamer{jobSystem};

        std::unique_ptr<Engine::InputBackend> backend{std::make_unique<Engine::InputBackendWindows>()};
        Engine::InputManager inputManager{std::move(backend)};

        std::vector<std::unique_ptr<Buffer>> sceneUboBuffers;
        uint32_t blueNoiseSlot;
        TextureCubeMap skyBox;
        std::unique_ptr<IBL> ibl;
        Camera camera{};

        CullPassNode cullPass{device, renderer, megaBuffer};
        VisibilityPassNode visPass {device, renderer, megaBuffer, cullPass};
        MaterialPassNode materialPass {device, renderer, megaBuffer, resourceHeap, renderGraph};
        SsaoPassNode ssaoPass {device, renderer, megaBuffer, resourceHeap};
        FxaaPassNode fxaaPass {device, renderer, megaBuffer, resourceHeap};

        int currentFrame;
        uint32_t imgIdx;
        VkExtent2D currentExtent;
        VkExtent2D lastExtent;
        bool graphCompiled;
        bool sceneGraphDirty;


        bool enableSSAO = true;
        bool ssaoKeyPressed = false;

        float fpsTimer = 0.0f;
        int fpsCount = 0;
    };
} // namespace Engine