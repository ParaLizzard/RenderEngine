#pragma once

#include <limits>
#include <unordered_map>
#include <format>

#include "SubsystemRegistry.h"
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
#include "System/Window/IWindow.h"
#include "System/Input/InputSubsystem.h"
#include "AssetSystem/IBL.h"
#include "Renderer/Passes/CullPassNode.h"
#include "Renderer/Passes/TransformUpdatePassNode.h"
#include "Renderer/Passes/FxaaPassNode.h"
#include "Renderer/Passes/TonemapPassNode.h"
#include "Renderer/Passes/MaterialPassNode.h"
#include "Renderer/Passes/SsaoPassNode.h"
#include "Renderer/Passes/TaaPassNode.h"
#include "Renderer/Passes/VisibilityPassNode.h"
#include "Renderer/Passes/CsmPassNode.h"
#include "Renderer/Passes/HiZPassNode.h"
#include "Renderer/RenderSettings.h"
#include "System/Window/WindowSubsystem.h"

namespace Engine {
    class Application
    {
    public:
        static constexpr int WIDTH = 3840;
        static constexpr int HEIGHT = 2120;

        Application();
        explicit Application(IWindow &window);
        ~Application();

        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;

        void run();

    private:
        void initScene();
        void compileFrameGraph();
        void updateFrameGraph();
        void updateSceneGraph();

        IWindow &window;
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

        std::vector<std::unique_ptr<Buffer>> sceneUboBuffers;
        uint32_t blueNoiseSlot;
        TextureCubeMap skyBox;
        std::unique_ptr<IBL> ibl;
        Camera camera{};

        TransformUpdatePassNode transformPass {device, renderer};
        CullPassNode cullPassPhase1 {device, renderer, megaBuffer, resourceHeap, 0};
        VisibilityPassNode visPassPhase1 {device, renderer, megaBuffer, cullPassPhase1, resourceHeap, 0};
        HiZPassNode hiZPass {device, renderer, megaBuffer, resourceHeap};
        CullPassNode cullPassPhase2 {device, renderer, megaBuffer, resourceHeap, 1, &cullPassPhase1};
        VisibilityPassNode visPassPhase2 {device, renderer, megaBuffer, cullPassPhase2, resourceHeap, 1};
        CsmPassNode csmPass {device, renderer, megaBuffer, resourceHeap, cullPassPhase1};
        MaterialPassNode materialPass {device, renderer, megaBuffer, resourceHeap, cullPassPhase1, renderGraph};
        SsaoPassNode ssaoPass {device, renderer, megaBuffer, resourceHeap};
        TaaPassNode taaPass {device, renderer, megaBuffer, resourceHeap};
        TonemapPassNode tonemapPass {device, renderer, megaBuffer, resourceHeap};
        FxaaPassNode fxaaPass {device, renderer, megaBuffer, resourceHeap};

        int currentFrame;
        uint32_t imgIdx;
        VkExtent2D currentExtent;
        VkExtent2D lastExtent;
        bool graphCompiled;
        bool sceneGraphDirty;


        bool enableSSAO = true;
        bool ssaoKeyPressed = false;
        
        bool freezeCulling = false;
        bool cullEnabled = true;
        glm::mat4 frozenView = glm::mat4(1.0f);
        glm::mat4 frozenViewProj = glm::mat4(1.0f);
        glm::vec3 frozenCameraPos = glm::vec3(0.0f);
        int debugViewMode = 0;
        int debugHiZMipLevel = 0;
        glm::mat4 prevViewProj = glm::mat4(1.0f);
        bool firstFrame = true;

        size_t aaCallbackToken = 0;
        size_t ssaoCallbackToken = 0;
    };
} // namespace Engine