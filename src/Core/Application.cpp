#include "Application.h"
#include "Passes/ForwardPassNode.h"
#include "Scene/LoaderGLTF.h"

namespace Engine
{
    static void updateHierarchy(GameObject& obj, std::vector<GameObject>& allObjects, const glm::mat4& parentMatrix)
    {
        // 1. My World Matrix = Parent's World Matrix * My Local Matrix
        obj.currentWorldMatrix = parentMatrix * obj.transform.mat4();

        // 2. Cascade down to all children
        for (auto childId : obj.childrenIds)
        {
            // Find the child in our vector by its ID
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
        Texture2D testTexture;
        testTexture.createDefaultTexture(&device, 255, 0, 0, 255, resourceHeap);
        ResourceHeap::TextureHandle testHandle = resourceHeap.registerTexture(testTexture.descriptor);
        resourceHeap.flushPendingUpdates();

        std::cout << "Stage 2 Verification: Texture registered at index " << testHandle.index << std::endl;
        testTexture.destroy();
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        Camera camera{};
        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        ForwardPassNode forwardPass{device, renderer, megaBuffer, resourceHeap};

        auto loaded = LoaderGLTF::loadObjectGLTF(device, "models/pbr_sphere.glb", megaBuffer, resourceHeap,
                                                 sceneTextures);

        for (auto& obj : loaded)
            gameObjects.push_back(std::move(obj));

        resourceHeap.uploadMaterialBuffer();
        resourceHeap.writeMaterialDescriptor();

        while (!window.shouldClose())
        {
            glfwPollEvents();

            float time = glfwGetTime();

            for (auto& obj : gameObjects)
            {
                if (obj.parentId == GameObject::INVALID_ID)
                {
                    obj.transform.rotation = glm::angleAxis(time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
                }
            }

            glm::mat4 identityMatrix = glm::mat4(1.0f);
            for (auto& obj : gameObjects)
            {
                if (obj.parentId == GameObject::INVALID_ID)
                {
                    updateHierarchy(obj, gameObjects, identityMatrix);
                }
            }

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE) continue;

            VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
            uint32_t imgIdx = renderer.getCurrentImageIndex();

            renderGraph.registerPhysicalBuffer(
                "MaterialSSBO",
                resourceHeap.getMaterialBufferInfo().buffer,
                resourceHeap.getMaterialBufferSize(),
                VK_PIPELINE_STAGE_2_HOST_BIT,
                VK_ACCESS_2_HOST_WRITE_BIT
            );

            renderGraph.registerPhysicalImage(
                "SwapChainImage",
                renderer.getSwapChain().getImage(imgIdx),
                renderer.getSwapChain().getImageView(imgIdx),
                renderer.getSwapChain().getSwapChainImageFormat(),
                currentExtent,
                VK_IMAGE_LAYOUT_UNDEFINED
            );

            renderGraph.registerPhysicalImage(
                "DepthImage",
                renderer.getSwapChain().getDepthImage(),
                renderer.getSwapChain().getDepthImageView(),
                renderer.getSwapChain().getDepthFormat(),
                currentExtent,
                VK_IMAGE_LAYOUT_UNDEFINED
            );

            float aspect = renderer.getaspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.0f), aspect, 0.1f, 100.0f);

            FrameInfo info{
                .frameIndex = static_cast<int>(imgIdx),
                .frameTime = time,
                .extent = currentExtent,
                .commandBuffer = cmd,
                .camera = camera,
                .gameObjects = gameObjects
            };

            renderGraph.addPass(&forwardPass);
            renderGraph.compile();
            renderGraph.execute(cmd, info);
            renderGraph.transitionToPresent(cmd, "SwapChainImage");
            renderGraph.clear();

            renderer.endFrame();
        }

        vkDeviceWaitIdle(device.getDevice());

        for (auto& tex : sceneTextures)
        {
            tex.destroy();
        }
    }
}
