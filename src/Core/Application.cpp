#include "Application.h"

#include <stb_image.h>

#include "Scene/IBL.h"
#include "Scene/LoaderGLTF.h"
#include "WindowWin32.h"

#include "Core/SceneManager.h"
#include "Core/EngineConfig.h"


namespace Engine {
    Application::Application()
    {}

    Application::~Application()
    {}

    void Application::run()
    {
        initScene();
        inputManager.Initialize(window);
        window.setInputManager(&inputManager);

        FrameInfo info{};
        info.device = &device;
        info.resourceHeap = &resourceHeap;
        info.renderer = &renderer;
        info.megaBuffer = &megaBuffer;
        info.renderGraph = &renderGraph;

        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        assetStreamer.enqueueLoad("models/pbr_sphere.glb");
        //assetStreamer.enqueueLoad("models/sponza_optimized.glb");
        //assetStreamer.enqueueLoad("C:/Users/Jan Varga/Downloads/pkg_a_curtains/pkg_a_curtains/NewSponza_Curtains_glTF.gltf");

        sceneManager.flattenSceneGraph();
        cullPass.markSceneDirty();

        cameraObject = std::make_shared<GameObject>(GameObject::createGameObject());
        cameraObject->transform.translation = {0.f, 0.f, -5.f};

        float lastTime = 0.0f;
        graphCompiled = false;
        sceneGraphDirty = true;
        lastExtent = {0, 0};

        while (!window.shouldClose()) {
            window.pollEvents();
            inputManager.Update();


            if (inputManager.IsKeyJustPressed(KeyCode::O)) {
                enableSSAO = !enableSSAO;
                std::cout << "SSAO: " << (enableSSAO ? "ON" : "OFF") << "\n";
            }


            auto currentTime = static_cast<float>(window.getTime());
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            double time = window.getTime();

            fpsTimer += deltaTime;
            fpsCount++;
            if (fpsTimer >= 1.0f) {
                std::string title = "Render Engine - " + std::to_string(fpsCount) + " FPS";
                window.setWindowTitle(title);
                fpsTimer -= 1.0f;
                fpsCount = 0;
            }

            std::vector<ParsedGLTF> parsedModels = assetStreamer.pollCompleted();
            sceneManager.integrateLoadedModels(device, parsedModels, megaBuffer, resourceHeap);

            cameraController.moveInPlaneXZ(inputManager, deltaTime, cameraObject);
            camera.setViewYXZ(cameraObject->transform.translation, cameraObject->transform.rotation);

            updateSceneGraph();

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE)
                continue;

            currentExtent = renderer.getSwapChain().getSwapChainExtent();
            imgIdx = renderer.getCurrentImageIndex();
            currentFrame = renderer.getFrameIndex();

            resourceHeap.update(currentFrame);

            SceneUbo uboData{};
            uboData.cameraPosition = glm::vec4(cameraObject->transform.translation, 1.0f);
            uboData.directionalLight = glm::vec4(glm::normalize(glm::vec3(0.f, -50.0f, 0.f)), 1.0f);
            uboData.maxReflectionLod = static_cast<float>(ibl->prefilteredCube.mipLevels - 1);
            uboData.blueNoiseTexIndex = blueNoiseSlot;

            sceneUboBuffers[currentFrame]->writeToBuffer(&uboData, sizeof(SceneUbo), 0);
            sceneUboBuffers[currentFrame]->flush(sizeof(SceneUbo), 0);

            compileFrameGraph();
            updateFrameGraph();

            float aspect = renderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.0f), aspect, 0.1f, 100.0f);

            info.frameIndex = currentFrame;
            info.frameTime = time;
            info.extent = currentExtent;
            info.commandBuffer = cmd;
            info.camera = &camera;
            info.gameObjects = &sceneManager.objects();
            info.jobSystem = &jobSystem;
            info.enableSSAO = enableSSAO;

            renderGraph.execute(cmd, info);
            renderGraph.transitionToPresent(cmd, "SwapChainImage");

            renderer.endFrame();
        }

        vkDeviceWaitIdle(device.getDevice());
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
        if (!noisePixels)
            throw std::runtime_error("Failed to load blue noise texture!");

        Texture2D blueNoiseTex;
        blueNoiseTex.fromBuffer(noisePixels,
                                noiseW * noiseH * 4,
                                VK_FORMAT_R8G8B8A8_UNORM,
                                noiseW,
                                noiseH,
                                &device,
                                resourceHeap,
                                VK_FILTER_NEAREST);
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

        for (int i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
            resourceHeap.uploadMaterialBuffer(i);
        }
        resourceHeap.writeMaterialDescriptorAllFrames();

        sceneUboBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++) {
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

        cullPass.markSceneDirty();
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

            renderGraph.registerPhysicalBuffer("CullCompactedIndirectCommands",
                                               cullPass.getCompactedIndirectBuffer(currentFrame),
                                               Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            renderGraph.registerPhysicalBuffer("CullDrawCount",
                                               cullPass.getDrawCountBuffer(currentFrame),
                                               sizeof(uint32_t),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);
            renderGraph.registerPhysicalBuffer("CullObjectData",
                                               cullPass.getGpuObjectBuffer(currentFrame),
                                               Config::MAX_SCENE_OBJECTS * sizeof(ObjectData),
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDeviceSize normalBufferSize =
                static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
            renderGraph.registerPhysicalBuffer("PackedNormals",
                                               materialPass.getPackedNormalBuffer(currentFrame),
                                               normalBufferSize,
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDeviceSize radianceBufferSize =
                static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
            renderGraph.registerPhysicalBuffer("PackedRadiances",
                                               materialPass.getPackedRadianceBuffer(currentFrame),
                                               radianceBufferSize,
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                               VK_ACCESS_2_SHADER_WRITE_BIT);

            VkDeviceSize worldPosBufferSize =
                static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(WorldData);
            renderGraph.registerPhysicalBuffer("WorldPosition",
                                               materialPass.getWorldPositionBuffer(currentFrame),
                                               worldPosBufferSize,
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

            renderGraph.addPass(&cullPass);
            renderGraph.addPass(&visPass);
            renderGraph.addPass(&materialPass);
            renderGraph.addPass(&ssaoPass);
            renderGraph.addPass(&fxaaPass);
            renderGraph.compile();

            graphCompiled = true;
            lastExtent = currentExtent;
        }
    }

    void Application::updateFrameGraph()
    {
        renderGraph.updateBufferHandle("MaterialSSBO",
                                       resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                       resourceHeap.getMaterialBufferSize());
        renderGraph.updateBufferHandle("CullCompactedIndirectCommands",
                                       cullPass.getCompactedIndirectBuffer(currentFrame),
                                       Config::MAX_SCENE_OBJECTS * sizeof(VkDrawIndexedIndirectCommand));
        renderGraph.updateBufferHandle(
            "CullObjectData",
            cullPass.getGpuObjectBuffer(currentFrame),
            Config::MAX_SCENE_OBJECTS * sizeof(ObjectData));

        renderGraph.updateBufferHandle(
            "CullDrawCount",
            cullPass.getDrawCountBuffer(currentFrame),
            sizeof(uint32_t));

        VkDeviceSize normalBufferSize =
            static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
        renderGraph.updateBufferHandle(
            "PackedNormals",
            materialPass.getPackedNormalBuffer(currentFrame),
            normalBufferSize);

        VkDeviceSize radianceBufferSize =
            static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
        renderGraph.updateBufferHandle(
            "PackedRadiances",
            materialPass.getPackedRadianceBuffer(currentFrame),
            radianceBufferSize);

        VkDeviceSize worldPosBufferSize =
            static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(WorldData);
        renderGraph.updateBufferHandle(
            "WorldPosition",
            materialPass.getWorldPositionBuffer(currentFrame),
            worldPosBufferSize);
        renderGraph.updateImageHandle("SwapChainImage",
                                      renderer.getSwapChain().getImage(imgIdx),
                                      renderer.getSwapChain().getImageView(imgIdx),
                                      currentExtent);
        renderGraph.updateImageHandle("DepthImage",
                                      renderer.getSwapChain().getDepthImage(),
                                      renderer.getSwapChain().getDepthImageView(),
                                      currentExtent);
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
