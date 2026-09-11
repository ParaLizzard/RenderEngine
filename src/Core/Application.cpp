#include "Core/Application.h"

#include <chrono>
#include <iostream>
#include <vulkan/vulkan.h>

#include <stb_image.h>

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Engine.h"
#include "AssetSystem/IBL.h"
#include "AssetSystem/LoaderGLTF.h"
#include "Scene/SceneManager.h"
#include "Core/EngineConstants.h"
#include "System/Input/InputSubsystem.h"
#include "System/Window/WindowSubsystem.h"
#include "System/Events/EventDispatcher.h"


namespace Engine {
    static IWindow& getWindowFromSubsystems()
    {
        auto& registry = Engine::Get().GetSubsystems();
        if (!registry.Has<WindowSubsystem>()) {
            registry.Register<WindowSubsystem>(WindowProps{
                .title = "RenderEngine",
                .width = Application::WIDTH,
                .height = Application::HEIGHT,
                .vsync = false,
                .fullscreen = false,
                .resizable = true,
                .cursorMode = CursorMode::Normal
            });
            registry.Get<WindowSubsystem>().Initialize(registry);
        }
        if (!registry.Has<InputSubsystem>()) {
            registry.Register<InputSubsystem, WindowSubsystem>();
            registry.Get<InputSubsystem>().Initialize(registry);
        }
        return registry.Get<WindowSubsystem>().GetWindow();
    }

    Application::Application()
        : window(getWindowFromSubsystems())
        , device(window)
        , renderer(window, device)
    {
    }

    Application::Application(IWindow &windowRef)
        : window(windowRef)
        , device(window)
        , renderer(window, device)
    {
    }

    Application::~Application()
    {
        vkDeviceWaitIdle(device.GetHandle());
    }

    void Application::run()
    {
        initScene();

        FrameInfo info{};
        info.device = &device;
        info.resourceHeap = &resourceHeap;
        info.renderer = &renderer;
        info.megaBuffer = &megaBuffer;
        info.renderGraph = &renderGraph;
        //info.input = &inputManager;

        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        //assetStreamer.enqueueLoad("models/pbr_sphere.glb");
        //assetStreamer.enqueueLoad("models/square.glb");
        assetStreamer.enqueueLoad("models/sponza_optimized.glb");
        //assetStreamer.enqueueLoad("models/model.glb");
        //assetStreamer.enqueueLoad("models/AlphaTest.glb");
         assetStreamer.enqueueLoad("C:/Users/Jan Varga/Downloads/pkg_a_curtains/pkg_a_curtains/NewSponza_Curtains_glTF.gltf");
        //assetStreamer.enqueueLoad("C:/Users/Jan Varga/Downloads/Sponza-crytek/Sponza.gltf");



        sceneManager.flattenSceneGraph();
        cullPassPhase1.markSceneDirty();
        cullPassPhase2.markSceneDirty();
        csmPass.markSceneDirty();

        cameraObject = std::make_shared<GameObject>(GameObject::createGameObject());
        cameraObject->transform.translation = {0.f, 0.f, -5.f};

        graphCompiled = false;
        sceneGraphDirty = true;
        lastExtent = {0, 0};

        Engine& engine = Engine::Get();
        SubsystemRegistry& registry = engine.GetSubsystems();
        auto& windowSubsystem = registry.Get<WindowSubsystem>();
        IWindow& window = windowSubsystem.GetWindow();
        InputSubsystem& input = registry.Get<InputSubsystem>();

        // Movement Axes
        input.BindAxis("MoveForward", { .positiveKey = KeyCode::W, .negativeKey = KeyCode::S, .scale = 1.0f });
        input.BindAxis("MoveRight",   { .positiveKey = KeyCode::D, .negativeKey = KeyCode::A, .scale = 1.0f });
        input.BindAxis("MoveUp",      { .positiveKey = KeyCode::E, .negativeKey = KeyCode::Q, .scale = 1.0f });

        // Key Actions
        input.BindAction("ToggleSSAO",       { .primaryKey = KeyCode::O,  .triggerType = TriggerType::JustPressed });
        input.BindAction("ToggleFreezeCull", { .primaryKey = KeyCode::F4, .triggerType = TriggerType::JustPressed });
        input.BindAction("ToggleCulling",    { .primaryKey = KeyCode::F5, .triggerType = TriggerType::JustPressed });
        input.BindAction("ToggleHiZDebug",   { .primaryKey = KeyCode::F6, .triggerType = TriggerType::JustPressed });

        aaCallbackToken = CVarAAMethod.OnChanged([this](int, int) {
            graphCompiled = false;
        });
        ssaoCallbackToken = CVarSSAOEnabled.OnChanged([this](bool, bool) {
            graphCompiled = false;
        });

        while (!window.ShouldClose()) {
            EventDispatcher::Get().DispatchQueuedEvents();
            window.PollEvents();

            if (input.IsActionTriggered("ToggleSSAO")) {
                CVarSSAOEnabled.Set(!CVarSSAOEnabled.Get());
                LOG_INFO("Application", "SSAO: {}", CVarSSAOEnabled.Get() ? "ON" : "OFF");
            }

            if (input.IsActionTriggered("ToggleFreezeCull")) {
                CVarFreezeCulling.Set(!CVarFreezeCulling.Get());
                if (CVarFreezeCulling.Get()) {
                    frozenViewProj = camera.getProjection() * camera.getView();
                    frozenCameraPos = camera.getPosition();
                    frozenView = camera.getView();
                }
                LOG_INFO("Application", "Freeze Culling: {}", CVarFreezeCulling.Get() ? "ON" : "OFF");
            }

            if (input.IsActionTriggered("ToggleCulling")) {
                cullEnabled = !cullEnabled;
                LOG_INFO("Application", "Culling: {}", cullEnabled ? "ON" : "OFF");
            }

            if (input.IsActionTriggered("ToggleHiZDebug")) {
                int mode = CVarHiZDebugMode.Get() == 1 ? 0 : 1;
                CVarHiZDebugMode.Set(mode);
                LOG_INFO("Application", "Hi-Z Debug View: {}", mode == 1 ? "ON (Mip 0)" : "OFF");
            }

            if (CVarHiZDebugMode.Get() == 1) {
                if (input.IsKeyJustPressed(KeyCode::PageUp) || input.IsKeyJustPressed(KeyCode::Right)) {
                    CVarHiZDebugMip.Set(std::min(CVarHiZDebugMip.Get() + 1, 11));
                    LOG_INFO("Application", "Hi-Z Debug Mip Level: {}", CVarHiZDebugMip.Get());
                }
                if (input.IsKeyJustPressed(KeyCode::PageDown) || input.IsKeyJustPressed(KeyCode::Left)) {
                    CVarHiZDebugMip.Set(std::max(CVarHiZDebugMip.Get() - 1, 0));
                    LOG_INFO("Application", "Hi-Z Debug Mip Level: {}", CVarHiZDebugMip.Get());
                }
            }

            auto& clock = engine.GetClock();
            clock.Tick();
            float deltaTime = clock.GetDeltaTime();
            [[maybe_unused]] float gameDt = clock.GetGameDeltaTime();
            float fps = clock.GetAverageFPS();

            std::string title = std::format("Render Engine - {:.1f} FPS", fps);
            window.setWindowTitle(title);

            std::vector<ParsedGLTF> parsedModels = assetStreamer.pollCompleted();
            for (auto& parsedModel : parsedModels) {
                auto newObjects = LoaderGLTF::finalize(parsedModel, device, megaBuffer, resourceHeap, sceneManager.textures());
                sceneManager.addGameObjects(std::move(newObjects));
                resourceHeap.markMaterialsDirty();
                megaBuffer.uploadToGPU();
                
                resourceHeap.setGeometryBuffers(
                    megaBuffer.getPositionBuffer(),
                    megaBuffer.getAttributeBuffer(),
                    megaBuffer.getIndexBuffer(),
                    megaBuffer.getMeshletBuffer(),
                    megaBuffer.getMeshletVerticesBuffer(),
                    megaBuffer.getMeshletTrianglesBuffer()
                );
                
                renderGraph.markSceneDirty();
                sceneGraphDirty = true;
                
                LOG_INFO("AssetStreamer", "Successfully streamed in async model!");
            }

            cameraController.moveInPlaneXZ(input, deltaTime, cameraObject);
            camera.setViewYXZ(cameraObject->transform.translation, cameraObject->transform.rotation);

            float aspect = renderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(30.0f), aspect, 0.1f, 100.0f);

            updateSceneGraph();

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE)
                continue;

            currentExtent = renderer.getSwapChain().getSwapChainExtent();
            imgIdx = renderer.getCurrentImageIndex();
            currentFrame = renderer.getFrameIndex();

            resourceHeap.update(currentFrame);

            info.camera = &camera;
            info.gameObjects = &sceneManager.objects();

            glm::mat4 proj = camera.getProjection();
            glm::vec2 subpixelJitter{0.0f, 0.0f};
            if (CVarAAMethod.Get() == 2) {
                static const glm::vec2 halton8[8] = {
                    {  0.0f,       -0.1666667f },
                    { -0.25f,       0.1666667f },
                    {  0.25f,      -0.3888889f },
                    { -0.375f,     -0.0555556f },
                    {  0.125f,      0.2777778f },
                    { -0.125f,     -0.2777778f },
                    {  0.375f,      0.0555556f },
                    { -0.4375f,     0.3888889f }
                };

                static uint64_t accumulatedFrameCount = 0;
                accumulatedFrameCount++;

                subpixelJitter = halton8[accumulatedFrameCount % 8];

                glm::vec2 jitterNDC = {
                    (2.0f * subpixelJitter.x) / static_cast<float>(currentExtent.width),
                    (2.0f * subpixelJitter.y) / static_cast<float>(currentExtent.height)
                };

                proj[2][0] += jitterNDC.x;
                proj[2][1] += jitterNDC.y;
            }

            glm::mat4 curViewProj = proj * camera.getView();
            if (firstFrame) {
                prevViewProj = curViewProj;
                firstFrame = false;
            }

            SceneUbo uboData{};
            uboData.viewProjection = curViewProj;
            uboData.prevViewProjection = prevViewProj;

            bool freezeCull = CVarFreezeCulling.Get();
            glm::mat4 cullVP = freezeCull ? frozenViewProj : curViewProj;
            glm::mat4 tvp = glm::transpose(cullVP);
            uboData.frustumPlanes[0] = tvp[3] + tvp[0]; // Left
            uboData.frustumPlanes[1] = tvp[3] - tvp[0]; // Right
            uboData.frustumPlanes[2] = tvp[3] + tvp[1]; // Bottom
            uboData.frustumPlanes[3] = tvp[3] - tvp[1]; // Top
            uboData.frustumPlanes[4] = tvp[2];          // Near
            uboData.frustumPlanes[5] = tvp[3] - tvp[2]; // Far

            for (int i = 0; i < 6; i++) {
                float len = glm::length(glm::vec3(uboData.frustumPlanes[i]));
                uboData.frustumPlanes[i] /= len;
            }

            glm::vec3 cullCamPos = freezeCull ? frozenCameraPos : camera.getPosition();
            uboData.cameraPosition = glm::vec4(cullCamPos, camera.getProjection()[1][1]);
            uboData.directionalLight = glm::vec4(glm::normalize(glm::vec3(0.2f, -1.0f, 0.1f)), 7.0f);
            uboData.maxReflectionLod = static_cast<float>(ibl->prefilteredCube.mipLevels - 1);
            uboData.blueNoiseTexIndex = blueNoiseSlot;

            csmPass.updateCascades(uboData, info);

            sceneUboBuffers[currentFrame]->writeToBuffer(&uboData, sizeof(SceneUbo), 0);
            sceneUboBuffers[currentFrame]->flush(sizeof(SceneUbo), 0);

            prevViewProj = curViewProj;

            compileFrameGraph();
            updateFrameGraph();

            info.frameIndex = currentFrame;
            info.frameTime = deltaTime;
            info.extent = currentExtent;
            info.commandBuffer = cmd;
            info.jobSystem = &jobSystem;
            info.enableSSAO = CVarSSAOEnabled.Get();
            info.input = &input;
            info.subpixelJitter = subpixelJitter;
            info.curViewProj = curViewProj;
            
            info.cullViewProj = freezeCull ? frozenViewProj : (camera.getProjection() * camera.getView());
            info.cullCameraPos = freezeCull ? frozenCameraPos : camera.getPosition();
            info.cullView = freezeCull ? frozenView : camera.getView();
            info.cullEnabled = cullEnabled;
            info.firstFrame = firstFrame;
            info.debugViewMode = CVarHiZDebugMode.Get();
            info.debugHiZMipLevel = CVarHiZDebugMip.Get();

            renderGraph.execute(cmd, info);
            renderGraph.transitionToPresent(cmd, "SwapChainImage");

            renderer.endFrame();

            input.Update(deltaTime);
        }

        if (aaCallbackToken != 0) {
            CVarAAMethod.RemoveCallback(aaCallbackToken);
            aaCallbackToken = 0;
        }
        if (ssaoCallbackToken != 0) {
            CVarSSAOEnabled.RemoveCallback(ssaoCallbackToken);
            ssaoCallbackToken = 0;
        }
    }

    void Application::initScene()
    {
        auto cubeFuture = LoaderGLTF::loadAsync(jobSystem, "models/cube.glb");
        ParsedGLTF cubeParsed = cubeFuture.get();
        auto cube = LoaderGLTF::finalize(cubeParsed, device, megaBuffer, resourceHeap, sceneManager.textures());

        GameObject* localCubeMeshNode = &cube[0];
        for (auto &node: cube) {
            if (node.subMesh.indexCount > 0) {
                localCubeMeshNode = &node;
                break;
            }
        }

        int noiseW, noiseH, noiseC;
        stbi_uc *noisePixels = stbi_load("assets/blue_noise.png", &noiseW, &noiseH, &noiseC, STBI_rgb_alpha);
        ENGINE_VERIFY(noisePixels != nullptr, "Failed to load blue noise texture!");

        Texture2D blueNoiseTex;
        blueNoiseTex.fromBuffer(noisePixels,
                                noiseW * noiseH * 4,
                                VK_FORMAT_R8G8B8A8_UNORM,
                                noiseW,
                                noiseH,
                                &device,
                                resourceHeap,
                                VK_FILTER_LINEAR,
                                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
        stbi_image_free(noisePixels);
        blueNoiseSlot = blueNoiseTex.heapHandle.index;
        sceneManager.textures().push_back(std::move(blueNoiseTex));

        std::array<std::string, 6> skyboxFaces = {
            "assets/px.png", // Layer 0: Positive X (Right)
            "assets/nx.png", // Layer 1: Negative X (Left)
            "assets/py.png", // Layer 2: Positive Y (Top)
            "assets/ny.png", // Layer 3: Negative Y (Bottom)
            "assets/pz.png", // Layer 4: Positive Z (Front)
            "assets/nz.png" // Layer 5: Negative Z (Back)
        };

        skyBox.loadFromFileSTB(skyboxFaces,
                               VK_FORMAT_R8G8B8A8_SRGB,
                               &device,
                               resourceHeap,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        megaBuffer.uploadToGPU();

        resourceHeap.setGeometryBuffers(
            megaBuffer.getPositionBuffer(),
            megaBuffer.getAttributeBuffer(),
            megaBuffer.getIndexBuffer(),
            megaBuffer.getMeshletBuffer(),
            megaBuffer.getMeshletVerticesBuffer(),
            megaBuffer.getMeshletTrianglesBuffer()
        );
        resourceHeap.setObjectBuffer(transformPass.getGlobalObjectBuffers());

        ibl = std::make_unique<IBL>(device, skyBox, resourceHeap, megaBuffer, *localCubeMeshNode);

        VkDescriptorImageInfo irradianceInfo{};
        irradianceInfo.sampler = ibl->irradianceCube.sampler;
        irradianceInfo.imageView = ibl->irradianceCube.imageView;
        irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo prefilterInfo{};
        prefilterInfo.sampler = ibl->prefilteredCube.sampler;
        prefilterInfo.imageView = ibl->prefilteredCube.imageView;
        prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo brdfLutInfo{};
        brdfLutInfo.sampler = ibl->BRDFLUT.sampler;
        brdfLutInfo.imageView = ibl->BRDFLUT.imageView;
        brdfLutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        resourceHeap.writeIBLDescriptors(irradianceInfo, prefilterInfo, brdfLutInfo);

        for (int i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            resourceHeap.uploadMaterialBuffer(i);
        }
        resourceHeap.writeMaterialDescriptorAllFrames();

        sceneUboBuffers.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            sceneUboBuffers[i] = std::make_unique<Buffer>(device,
                                                          sizeof(SceneUbo),
                                                          1,
                                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                          VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                          0);
            VkDescriptorBufferInfo uboInfo = sceneUboBuffers[i]->descriptorInfo(sizeof(SceneUbo), 0);
            resourceHeap.writeSceneUboDescriptor(uboInfo, i);
        }

        sceneManager.flattenSceneGraph();

        transformPass.markSceneDirty();
        cullPassPhase1.markSceneDirty();
        cullPassPhase2.markSceneDirty();
        csmPass.markSceneDirty();
    }

   void Application::compileFrameGraph()
    {
        if (!graphCompiled || currentExtent.width != lastExtent.width ||
            currentExtent.height != lastExtent.height) {
            renderGraph.clear();


            renderGraph.registerPhysicalBuffer("MaterialSSBO",
                                               resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                               resourceHeap.getMaterialBufferInfo(currentFrame).range,
                                               VK_PIPELINE_STAGE_2_HOST_BIT,
                                               VK_ACCESS_2_HOST_WRITE_BIT);

            renderGraph.registerPhysicalBuffer("CompactedIndexBuffer",
                                   cullPassPhase1.getCompactedIndexBuffer(currentFrame),
                                   Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t),
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.registerPhysicalBuffer("SingleIndirectCommand",
                                               cullPassPhase1.getSingleIndirectCommandBuffer(currentFrame),
                                               sizeof(VkDrawIndirectCommand),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.registerPhysicalBuffer("MaskedCompactedIndexBuffer",
                                               cullPassPhase1.getMaskedCompactedIndexBuffer(currentFrame),
                                               Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.registerPhysicalBuffer("MaskedSingleIndirectCommand",
                                               cullPassPhase1.getMaskedSingleIndirectCommandBuffer(currentFrame),
                                               sizeof(VkDrawIndirectCommand),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.registerPhysicalImage("SwapChainImage",
                                              renderer.getSwapChain().getImage(imgIdx),
                                              renderer.getSwapChain().getImageView(imgIdx),
                                              renderer.getSwapChain().getSwapChainImageFormat(),
                                              currentExtent,
                                              VK_IMAGE_LAYOUT_UNDEFINED);
            renderGraph.registerPhysicalImage("DepthImage",
                                              renderer.getSwapChain().getDepthImage(),
                                              renderer.getSwapChain().getDepthImageView(),
                                              renderer.getSwapChain().getDepthFormat(),
                                              currentExtent,
                                              VK_IMAGE_LAYOUT_UNDEFINED);

            renderGraph.addPass(&transformPass);
            renderGraph.addPass(&cullPassPhase1);
            renderGraph.addPass(&visPassPhase1);
            renderGraph.addPass(&hiZPass);
            renderGraph.addPass(&cullPassPhase2);
            renderGraph.addPass(&visPassPhase2);
            renderGraph.addPass(&csmPass);
            renderGraph.addPass(&ssaoPass);
            renderGraph.addPass(&materialPass);
            if (CVarAAMethod.Get() == 2) {
                renderGraph.addPass(&taaPass);
            }
            renderGraph.addPass(&tonemapPass);
            if (CVarAAMethod.Get() == 1) {
                renderGraph.addPass(&fxaaPass);
            }

            FrameInfo frameInfo{};
            frameInfo.frameIndex = currentFrame;
            frameInfo.extent = currentExtent;
            frameInfo.renderGraph = &renderGraph;
            frameInfo.renderer = &renderer;
            frameInfo.device = &device;

            renderGraph.registerPassResources(frameInfo);
            renderGraph.compile();

            graphCompiled = true;
            lastExtent = currentExtent;
        }
    }

    void Application::updateFrameGraph()
    {
        FrameInfo frameInfo{};
        frameInfo.frameIndex = currentFrame;
        frameInfo.extent = currentExtent;
        frameInfo.renderGraph = &renderGraph;
        frameInfo.renderer = &renderer;
        frameInfo.device = &device;

        renderGraph.updateBufferHandle("MaterialSSBO",
                                       resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                       resourceHeap.getMaterialBufferSize());

        renderGraph.updateBufferHandle("CompactedIndexBuffer",
                                       cullPassPhase1.getCompactedIndexBuffer(currentFrame),
                                       Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t));

        renderGraph.updateBufferHandle("SingleIndirectCommand",
                                       cullPassPhase1.getSingleIndirectCommandBuffer(currentFrame),
                                       sizeof(VkDrawIndirectCommand));

        renderGraph.updateBufferHandle("MaskedCompactedIndexBuffer",
                                       cullPassPhase1.getMaskedCompactedIndexBuffer(currentFrame),
                                       Constants::MAX_SCENE_OBJECTS * Constants::MAX_MESHLET_TRIANGLES * 3 * sizeof(uint32_t));

        renderGraph.updateBufferHandle("MaskedSingleIndirectCommand",
                                       cullPassPhase1.getMaskedSingleIndirectCommandBuffer(currentFrame),
                                       sizeof(VkDrawIndirectCommand));

        renderGraph.updateImageHandle("SwapChainImage",
                                      renderer.getSwapChain().getImage(imgIdx),
                                      renderer.getSwapChain().getImageView(imgIdx),
                                      currentExtent);
        renderGraph.updateImageHandle("DepthImage",
                                      renderer.getSwapChain().getDepthImage(),
                                      renderer.getSwapChain().getDepthImageView(),
                                      currentExtent);

        renderGraph.updatePassResources(frameInfo);
    }

    void Application::updateSceneGraph()
    {
        if (sceneGraphDirty || sceneManager.isSceneGraphDirty()) {
            for (auto &obj: sceneManager.objects()) {
                if (obj.parentCacheIndex != std::numeric_limits<size_t>::max()) {
                    obj.currentWorldMatrix =
                        sceneManager.objects()[obj.parentCacheIndex].currentWorldMatrix * obj.transform.mat4();
                } else {
                    obj.currentWorldMatrix = obj.transform.mat4();
                }
            }
            renderGraph.markSceneDirty();
            sceneGraphDirty = false;
            sceneManager.markClean();
        }
    }
} // namespace Engine
