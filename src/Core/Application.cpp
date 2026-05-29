#include "Application.h"
#include "Passes/ForwardPassNode.h"

namespace Engine
{
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
        std::vector<GameObject> gameObjects;
        Camera camera{};
        camera.setViewTarget(glm::vec3{0.0f, 0.0f, -5.0f}, glm::vec3{0.0f, 0.0f, 0.0f});

        ForwardPassNode forwardPass{device, renderer, megaBuffer, resourceHeap};

        std::vector<Model::Vertex> vertices = {
            {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0},
            {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0}
        };
        std::vector<uint32_t> indices = {0, 1, 2};

        GameObject triangle = GameObject::createGameObject();
        triangle.subMesh = megaBuffer.registerMesh(vertices, indices);
        triangle.transform.translation = {0.0f, 0.0f, 0.0f};

        gameObjects.push_back(std::move(triangle));

        while (!window.shouldClose())
        {
            glfwPollEvents();

            VkCommandBuffer cmd = renderer.beginFrame();
            if (cmd == VK_NULL_HANDLE) continue;

            VkExtent2D currentExtent = renderer.getSwapChain().getSwapChainExtent();
            uint32_t imgIdx = renderer.getCurrentImageIndex();

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
                .frameTime = 0.016f,
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
    }
}
