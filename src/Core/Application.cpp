#include "Application.h"

#include <stb_image.h>

#include "Passes/FxaaPassNode.h"
#include "Passes/CullPassNode.h"
#include "Passes/MaterialPassNode.h"
#include "Passes/SsaoPassNode.h"
#include "Passes/VisibilityPassNode.h"
#include "Scene/IBL.h"
#include "Scene/LoaderGLTF.h"




namespace Engine
{
    static void flattenSceneGraph(std::vector<GameObject>& gameObjects)
    {
        std::vector<GameObject> sorted;
        sorted.reserve(gameObjects.size());

        std::unordered_map<unsigned int, size_t> originalIdToIndex;
        for (size_t i = 0; i < gameObjects.size(); ++i)
        {
            originalIdToIndex[gameObjects[i].getId()] = i;
        }

        std::vector<size_t> stack;
        for (size_t i = 0; i < gameObjects.size(); ++i)
        {
            if (gameObjects[i].parentId == GameObject::INVALID_ID)
            {
                stack.push_back(i);
            }
        }

        std::unordered_map<unsigned int, size_t> newIndexMap;

        while (!stack.empty())
        {
            size_t currIdx = stack.back();
            stack.pop_back();

            size_t newIdx = sorted.size();
            newIndexMap[gameObjects[currIdx].getId()] = newIdx;

            auto childrenIds = gameObjects[currIdx].childrenIds;

            sorted.push_back(std::move(gameObjects[currIdx]));

            for (auto childId : childrenIds)
            {
                if (originalIdToIndex.count(childId))
                {
                    stack.push_back(originalIdToIndex[childId]);
                }
            }
        }

        for (auto& obj : sorted)
        {
            if (obj.parentId != GameObject::INVALID_ID && newIndexMap.count(obj.parentId))
            {
                obj.parentCacheIndex = newIndexMap[obj.parentId];
            }
            else
            {
                obj.parentCacheIndex = std::numeric_limits<size_t>::max();
            }
        }

        gameObjects = std::move(sorted);
    }

    static void updateHierarchy(GameObject& obj, std::vector<GameObject>& allObjects, const glm::mat4& parentMatrix)
    {
        obj.currentWorldMatrix = parentMatrix * obj.transform.mat4();

        for (auto childId : obj.childrenIds)
        {
            auto it = std::find_if(allObjects.begin(), allObjects.end(),
                                   [childId](const GameObject& g) { return g.getId() == childId; });

            if (it != allObjects.end())
            {
                updateHierarchy(*it, allObjects, obj.currentWorldMatrix);
            }
        }
    }

    Application::Application()
    {

    }

    Application::~Application()
    {
        for (auto& tex : sceneTextures)
        {
            tex.destroy();
        }
    }

    void Application::run()
    {
        FrameInfo info{};
        info.device = &device;
        info.resourceHeap = &resourceHeap;
        info.renderer = &renderer;
        info.megaBuffer = &megaBuffer;
        info.renderGraph = &renderGraph;

        Camera camera{};
        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        CullPassNode cullPass(device, renderer, megaBuffer);
        VisibilityPassNode visPass{device,renderer,megaBuffer,cullPass};
        MaterialPassNode materialPass{device,renderer,megaBuffer,resourceHeap,renderGraph};
        SsaoPassNode ssaoPass{device, renderer, megaBuffer, resourceHeap};
        FxaaPassNode fxaaPass{device, renderer, megaBuffer, resourceHeap};


        std::vector<std::future<ParsedGLTF>> pendingLoads;
        //pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "models/pbr_sphere.glb"));
        //pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "C:/Users/Jan Varga/Downloads/main_sponza (1)/main_sponza/NewSponza_Main_glTF_003.gltf"));
        pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "models/sponza_optimized.glb"));
        pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "C:/Users/Jan Varga/Downloads/pkg_a_curtains/pkg_a_curtains/NewSponza_Curtains_glTF.gltf"));
        //pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "C:/Users/Jan Varga/Downloads/pkg_b_ivy1/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf"));
        //pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "C:/Users/Martin Varga/Downloads/metallic--roughness--test/source/Metallic_Roughness_Test.glb"));

        auto cubeFuture = LoaderGLTF::loadAsync(jobSystem, "models/cube.glb");
        ParsedGLTF cubeParsed = cubeFuture.get();
        auto cube = LoaderGLTF::finalize(cubeParsed, device, megaBuffer, resourceHeap, sceneTextures);

        GameObject* cubeMeshNode = &cube[0];
        for (auto& node : cube)
        {
            if (node.subMesh.indexCount > 0)
            {
                cubeMeshNode = &node;
                break;
            }
        }

        int noiseW, noiseH, noiseC;
        stbi_uc* noisePixels = stbi_load("assets/blue_noise.png", &noiseW, &noiseH, &noiseC, STBI_rgb_alpha);
        if (!noisePixels) throw std::runtime_error("Failed to load blue noise texture!");

        Texture2D blueNoiseTex;
        blueNoiseTex.fromBuffer(noisePixels, noiseW * noiseH * 4, VK_FORMAT_R8G8B8A8_UNORM, noiseW, noiseH, &device,
                                resourceHeap, VK_FILTER_NEAREST);
        stbi_image_free(noisePixels);
        uint32_t blueNoiseSlot = blueNoiseTex.heapHandle.index;
        sceneTextures.push_back(std::move(blueNoiseTex));

        std::array<std::string, 6> skyboxFaces = {
            "assets/px.png", // Layer 0: Positive X (Right)
            "assets/nx.png", // Layer 1: Negative X (Left)
            "assets/py.png", // Layer 2: Positive Y (Top)
            "assets/ny.png", // Layer 3: Negative Y (Bottom)
            "assets/pz.png", // Layer 4: Positive Z (Front)
            "assets/nz.png" // Layer 5: Negative Z (Back)
        };

        TextureCubeMap skyBox;

        skyBox.loadFromFileSTB(
            skyboxFaces,
            VK_FORMAT_R8G8B8A8_SRGB,
            &device,
            resourceHeap,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        megaBuffer.uploadToGPU();

        IBL ibl = IBL(device, skyBox, resourceHeap, megaBuffer, *cubeMeshNode);

        VkDescriptorImageInfo irradianceInfo{};
        irradianceInfo.sampler = ibl.irradianceCube.sampler;
        irradianceInfo.imageView = ibl.irradianceCube.imageView;
        irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo prefilterInfo{};
        prefilterInfo.sampler = ibl.prefilteredCube.sampler;
        prefilterInfo.imageView = ibl.prefilteredCube.imageView;
        prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo brdfLutInfo{};
        brdfLutInfo.sampler = ibl.BRDFLUT.sampler;
        brdfLutInfo.imageView = ibl.BRDFLUT.imageView;
        brdfLutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        resourceHeap.writeIBLDescriptors(irradianceInfo, prefilterInfo, brdfLutInfo);

        for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
            resourceHeap.uploadMaterialBuffer(i);
        }
        resourceHeap.writeMaterialDescriptorAllFrames();

        std::vector<std::unique_ptr<Buffer>> sceneUboBuffers(Renderer::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
            sceneUboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(SceneUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                0
            );
            VkDescriptorBufferInfo uboInfo = sceneUboBuffers[i]->descriptorInfo(sizeof(SceneUbo), 0);
            resourceHeap.writeSceneUboDescriptor(uboInfo, i);
        }

        flattenSceneGraph(gameObjects);
        cullPass.markSceneDirty();


        cameraObject = std::make_shared<GameObject>(GameObject::createGameObject());
        cameraObject->transform.translation = {0.f, 0.f, -5.f};

        float lastTime = 0.0f;
        bool graphCompiled = false;
        bool sceneGraphDirty = true;
        VkExtent2D lastExtent = {0, 0};

        while (!window.shouldClose())
        {
            glfwPollEvents();


            if (glfwGetKey(window.getGLFWWinodow(), GLFW_KEY_O) == GLFW_PRESS)
            {
                if (!ssaoKeyPressed)
                {
                    enableSSAO = !enableSSAO;
                    ssaoKeyPressed = true;
                    std::cout << "SSAO is now: " << (enableSSAO ? "ON" : "OFF") << "\n";
                }
            }
            else
            {
                ssaoKeyPressed = false;
            }

            float currentTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            float time = glfwGetTime();

            fpsTimer += deltaTime;
            fpsCount++;

            // 3. Print every 2 seconds
            if (fpsTimer >= 2.0f)
            {
                float avgFps = fpsCount / fpsTimer;
                std::cout << "Average FPS: " << avgFps << " (Frame time: "
                          << (fpsTimer / fpsCount) * 1000.0f << " ms)" << std::endl;
                fpsTimer = 0.0f;
                fpsCount = 0;
            }

            for (int i = pendingLoads.size() - 1; i >= 0; --i)
            {
                if (pendingLoads[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    vkDeviceWaitIdle(device.getDevice());

                    ParsedGLTF parsedData = pendingLoads[i].get();
                    auto newObjects = LoaderGLTF::finalize(parsedData, device, megaBuffer, resourceHeap, sceneTextures);

                    for (auto& obj : newObjects) gameObjects.push_back(std::move(obj));
                    flattenSceneGraph(gameObjects);
                    sceneGraphDirty = true;

                    resourceHeap.markMaterialsDirty();

                    megaBuffer.uploadToGPU();
                    cullPass.markSceneDirty();

                    std::cout << "Successfully streamed in async model!" << std::endl;

                    pendingLoads.erase(pendingLoads.begin() + i);
                }
            }



            cameraController.moveInPlaneXZ(window.getGLFWWinodow(), deltaTime, cameraObject);
            camera.setViewYXZ(cameraObject->transform.translation, cameraObject->transform.rotation);

            if (sceneGraphDirty)
            {
                for (auto& obj : gameObjects)
                {
                    if (obj.parentCacheIndex != std::numeric_limits<size_t>::max())
                    {
                        obj.currentWorldMatrix = gameObjects[obj.parentCacheIndex].currentWorldMatrix * obj.transform.
                            mat4();
                    }
                    else
                    {
                        obj.currentWorldMatrix = obj.transform.mat4();
                    }
                }
                cullPass.markSceneDirty();
                materialPass.markSceneDirty();
                sceneGraphDirty = false;
            }

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE) continue;

            VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
            uint32_t imgIdx = renderer.getCurrentImageIndex();
            int currentFrame = renderer.getFrameIndex();

            resourceHeap.update(currentFrame);

            SceneUbo uboData{};
            uboData.cameraPosition = glm::vec4(cameraObject->transform.translation, 1.0f);
            uboData.directionalLight = glm::vec4(glm::normalize(glm::vec3(0.f, -50.0f, 0.f)), 1.0f);
            uboData.maxReflectionLod = static_cast<float>(ibl.prefilteredCube.mipLevels - 1);
            uboData.blueNoiseTexIndex = blueNoiseSlot;

            sceneUboBuffers[currentFrame]->writeToBuffer(&uboData, sizeof(SceneUbo), 0);
            sceneUboBuffers[currentFrame]->flush(sizeof(SceneUbo), 0);

            if (!graphCompiled || currentExtent.width != lastExtent.width || currentExtent.height != lastExtent.height)
            {
                renderGraph.clear();


                renderGraph.registerPhysicalBuffer("MaterialSSBO",
                                                   resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                                   resourceHeap.getMaterialBufferInfo(currentFrame).range,
                                                   VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT);

                renderGraph.registerPhysicalBuffer("CullCompactedIndirectCommands",
                                       cullPass.getCompactedIndirectBuffer(currentFrame),
                                       100000 * sizeof(VkDrawIndexedIndirectCommand),
                                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                       VK_ACCESS_2_SHADER_WRITE_BIT);

                renderGraph.registerPhysicalBuffer("CullDrawCount",
                                                   cullPass.getDrawCountBuffer(currentFrame),
                                                   sizeof(uint32_t),
                                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                   VK_ACCESS_2_SHADER_WRITE_BIT);
                renderGraph.registerPhysicalBuffer("CullObjectData",
                                   cullPass.getGpuObjectBuffer(currentFrame),
                                   100000 * sizeof(ObjectData),
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);

                VkDeviceSize normalBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
                renderGraph.registerPhysicalBuffer("PackedNormals", materialPass.getPackedNormalBuffer(currentFrame), normalBufferSize,
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);

                VkDeviceSize radianceBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
                renderGraph.registerPhysicalBuffer("PackedRadiances", materialPass.getPackedRadianceBuffer(currentFrame), radianceBufferSize,
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);

                VkDeviceSize worldPosBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(WorldData);
                renderGraph.registerPhysicalBuffer("WorldPosition",
                                   materialPass.getWorldPositionBuffer(currentFrame),
                                   worldPosBufferSize,
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                   VK_ACCESS_2_SHADER_WRITE_BIT);
                renderGraph.registerPhysicalImage("SwapChainImage",
                                                  renderer.getSwapChain().getImage(imgIdx),
                                                  renderer.getSwapChain().getImageView(imgIdx),
                                                  renderer.getSwapChain().getSwapChainImageFormat(), currentExtent,
                                                  VK_IMAGE_LAYOUT_UNDEFINED);
                renderGraph.registerPhysicalImage("DepthImage",
                                                  renderer.getSwapChain().getDepthImage(),
                                                  renderer.getSwapChain().getDepthImageView(),
                                                  renderer.getSwapChain().getDepthFormat(), currentExtent,
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

            renderGraph.updateBufferHandle("MaterialSSBO",
                                           resourceHeap.getMaterialBufferInfo(currentFrame).buffer,
                                           resourceHeap.getMaterialBufferSize());
            renderGraph.updateBufferHandle("CullCompactedIndirectCommands",
                               cullPass.getCompactedIndirectBuffer(currentFrame),
                               100000 * sizeof(VkDrawIndexedIndirectCommand));
            renderGraph.updateBufferHandle("CullObjectData",
                               cullPass.getGpuObjectBuffer(currentFrame),
                               100000 * sizeof(ObjectData));

            renderGraph.updateBufferHandle("CullDrawCount",
                                           cullPass.getDrawCountBuffer(currentFrame),
                                           sizeof(uint32_t));


            VkDeviceSize normalBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
            renderGraph.updateBufferHandle("PackedNormals", materialPass.getPackedNormalBuffer(currentFrame), normalBufferSize);

            VkDeviceSize radianceBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(uint32_t);
            renderGraph.updateBufferHandle("PackedRadiances", materialPass.getPackedRadianceBuffer(currentFrame), radianceBufferSize);

            VkDeviceSize worldPosBufferSize = static_cast<VkDeviceSize>(currentExtent.width) * currentExtent.height * sizeof(WorldData);
            renderGraph.updateBufferHandle("WorldPosition", materialPass.getWorldPositionBuffer(currentFrame), worldPosBufferSize);
            renderGraph.updateImageHandle("SwapChainImage",
                                          renderer.getSwapChain().getImage(imgIdx),
                                          renderer.getSwapChain().getImageView(imgIdx), currentExtent);
            renderGraph.updateImageHandle("DepthImage",
                                          renderer.getSwapChain().getDepthImage(),
                                          renderer.getSwapChain().getDepthImageView(), currentExtent);

            float aspect = renderer.getaspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.0f), aspect, 0.1f, 100.0f);


                info.frameIndex = currentFrame;
                info.frameTime = time;
                info.extent = currentExtent;
                info.commandBuffer = cmd;
                info.camera = &camera;
                info.gameObjects = &gameObjects;
                info.jobSystem = &jobSystem;
                info.enableSSAO = enableSSAO;


            renderGraph.execute(cmd, info);
            renderGraph.transitionToPresent(cmd, "SwapChainImage");

            renderer.endFrame();
        }

        vkDeviceWaitIdle(device.getDevice());
    }
}
