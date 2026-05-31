#include "Application.h"
#include "Passes/ForwardPassNode.h"
#include "Passes/FxaaPassNode.h"
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
    }

    void Application::run()
    {
        Camera camera{};
        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        ForwardPassNode forwardPass{device, renderer, megaBuffer, resourceHeap};
        FxaaPassNode fxaaPass{device, renderer, megaBuffer, resourceHeap};

        std::vector<std::future<ParsedGLTF>> pendingLoads;
        //pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "models/pbr_sphere.glb"));
        pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, "C:/Users/Martin Varga/Downloads/metallic--roughness--test/source/Metallic_Roughness_Test.glb"));

        auto cubeFuture = LoaderGLTF::loadAsync(jobSystem, "models/cube.glb");
        ParsedGLTF cubeParsed = cubeFuture.get();
        auto cube = LoaderGLTF::finalize(cubeParsed, device, megaBuffer, resourceHeap, sceneTextures);

        GameObject* cubeMeshNode = &cube[0];
        for (auto& node : cube) {
            if (node.subMesh.indexCount > 0) {
                cubeMeshNode = &node;
                break;
            }
        }

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

        IBL ibl = IBL(device, skyBox, resourceHeap, megaBuffer, *cubeMeshNode);

        VkDescriptorImageInfo irradianceInfo{
            ibl.irradianceCube.sampler, ibl.irradianceCube.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo prefilterInfo{
            ibl.prefilteredCube.sampler, ibl.prefilteredCube.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        VkDescriptorImageInfo brdfInfo{
            ibl.BRDFLUT.sampler, ibl.BRDFLUT.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        resourceHeap.writeIBLDescriptors(irradianceInfo, prefilterInfo, brdfInfo);

        resourceHeap.uploadMaterialBuffer();
        resourceHeap.writeMaterialDescriptor();

        std::unique_ptr<Buffer> sceneUboBuffer = std::make_unique<Buffer>(
            device,
            sizeof(SceneUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            0
        );

        VkDescriptorBufferInfo uboInfo = sceneUboBuffer->descriptorInfo(sizeof(SceneUbo), 0);
        resourceHeap.writeSceneUboDescriptor(uboInfo);

        flattenSceneGraph(gameObjects);

        cameraObject = std::make_shared<GameObject>(GameObject::createGameObject());
        cameraObject->transform.translation = {0.f, 0.f, -5.f};

        float lastTime = 0.0f;
        bool graphCompiled = false;
        VkExtent2D lastExtent = {0, 0};

        while (!window.shouldClose())
        {
            glfwPollEvents();

            float currentTime = static_cast<float>(glfwGetTime());
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            float time = glfwGetTime();

            for (int i = pendingLoads.size() - 1; i >= 0; --i)
            {
                if (pendingLoads[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    ParsedGLTF parsedData = pendingLoads[i].get();

                    auto newObjects = LoaderGLTF::finalize(parsedData, device, megaBuffer, resourceHeap, sceneTextures);

                    for (auto& obj : newObjects) gameObjects.push_back(std::move(obj));

                    flattenSceneGraph(gameObjects);

                    resourceHeap.uploadMaterialBuffer();
                    resourceHeap.writeMaterialDescriptor();

                    std::cout << "Successfully streamed in async model!" << std::endl;

                    pendingLoads.erase(pendingLoads.begin() + i);
                }
            }

            SceneUbo uboData{};
            uboData.cameraPosition = glm::vec4(cameraObject->transform.translation, 1.0f);
            uboData.directionalLight = glm::vec4(glm::normalize(glm::vec3(cos(time), -1.0f, sin(time))), 1.0f);
            sceneUboBuffer->writeToBuffer(&uboData, sizeof(SceneUbo), 0);
            sceneUboBuffer->flush(sizeof(SceneUbo), 0);

            cameraController.moveInPlaneXZ(window.getGLFWWinodow(), deltaTime, cameraObject);
            camera.setViewYXZ(cameraObject->transform.translation, cameraObject->transform.rotation);

            for (auto& obj : gameObjects)
            {
                if (obj.parentCacheIndex != std::numeric_limits<size_t>::max())
                {
                    obj.currentWorldMatrix = gameObjects[obj.parentCacheIndex].currentWorldMatrix * obj.transform.mat4();
                }
                else
                {
                    obj.currentWorldMatrix = obj.transform.mat4();
                }
            }

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE) continue;

            VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
            uint32_t imgIdx = renderer.getCurrentImageIndex();
            int currentFrame = renderer.getFrameIndex();

            if (!graphCompiled || currentExtent.width != lastExtent.width || currentExtent.height != lastExtent.height)
            {
                renderGraph.clear();

                renderGraph.registerPhysicalBuffer("SceneUBO",
                    sceneUboBuffer->getBuffer(), sceneUboBuffer->getBufferSize(),
                    VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT);
                renderGraph.registerPhysicalBuffer("MaterialSSBO",
                    resourceHeap.getMaterialBufferInfo().buffer, resourceHeap.getMaterialBufferInfo().range,
                    VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT);

                renderGraph.registerPhysicalImage("SwapChainImage",
                    renderer.getSwapChain().getImage(imgIdx), renderer.getSwapChain().getImageView(imgIdx),
                    renderer.getSwapChain().getSwapChainImageFormat(), currentExtent, VK_IMAGE_LAYOUT_UNDEFINED);
                renderGraph.registerPhysicalImage("DepthImage",
                    renderer.getSwapChain().getDepthImage(), renderer.getSwapChain().getDepthImageView(),
                    renderer.getSwapChain().getDepthFormat(), currentExtent, VK_IMAGE_LAYOUT_UNDEFINED);

                renderGraph.addPass(&forwardPass);
                renderGraph.addPass(&fxaaPass);
                renderGraph.compile();

                graphCompiled = true;
                lastExtent = currentExtent;
            }

            renderGraph.updateImageHandle("SwapChainImage",
                renderer.getSwapChain().getImage(imgIdx), renderer.getSwapChain().getImageView(imgIdx), currentExtent);
            renderGraph.updateImageHandle("DepthImage",
                renderer.getSwapChain().getDepthImage(), renderer.getSwapChain().getDepthImageView(), currentExtent);

            float aspect = renderer.getaspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.0f), aspect, 0.1f, 100.0f);

            FrameInfo info{
                .frameIndex = currentFrame,
                .frameTime = time,
                .extent = currentExtent,
                .commandBuffer = cmd,
                .camera = camera,
                .gameObjects = gameObjects,
                .renderGraph = &renderGraph
            };

            renderGraph.execute(cmd, info);
            renderGraph.transitionToPresent(cmd, "SwapChainImage");

            renderer.endFrame();
        }

        vkDeviceWaitIdle(device.getDevice());

        for (auto& tex : sceneTextures)
        {
            tex.destroy();
        }
    }
}
