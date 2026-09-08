#include "Renderer/Renderer.h"

#include "Core/Assert.h"
#include "Core/EngineConstants.h"

namespace Engine {
    Renderer::Renderer(IWindow &window, Device &device): window(window), device(device)
    {
        swapChain = std::make_unique<SwapChain>(device, window);

        createFrameData();

        resizeSubscription = EventDispatcher::Get().SubscribeScoped<WindowResizeEvent>(
            [this](WindowResizeEvent& e) {
                this->onWindowResize(e.GetWidth(), e.GetHeight());
            }
        );
    }

    Renderer::~Renderer()
    {
        vkDeviceWaitIdle(device.getDevice());

        for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyCommandPool(device.getDevice(), frames[i].commandPool, nullptr);
            vkDestroySemaphore(device.getDevice(), frames[i].imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(device.getDevice(), frames[i].renderFinishedSemaphore, nullptr);
            vkDestroyFence(device.getDevice(), frames[i].fence, nullptr);
        }
    }

    VkCommandBuffer Renderer::beginFrame()
    {
        ENGINE_ASSERT(!isFrameStarted, "Renderer: Frame already started");
        vkWaitForFences(device.getDevice(), 1, &frames[currentFrameIndex].fence, VK_TRUE, UINT64_MAX);

        swapChainRecreatedThisFrame = false;

        VkResult result =
            swapChain->acquireNextImage(frames[currentFrameIndex].imageAvailableSemaphore, &currentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return nullptr;
        }

        isFrameStarted = true;

        vkResetFences(device.getDevice(), 1, &frames[currentFrameIndex].fence);

        vkResetCommandBuffer(frames[currentFrameIndex].commandBuffer, 0);

        VkCommandBufferBeginInfo cmdInfo {};
        cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdInfo.pNext = nullptr;

        ENGINE_VERIFY(vkBeginCommandBuffer(frames[currentFrameIndex].commandBuffer, &cmdInfo) == VK_SUCCESS,
            "Renderer: failed to begin recording command buffers");

        return frames[currentFrameIndex].commandBuffer;
    }

    void Renderer::endFrame()
    {
        ENGINE_ASSERT(isFrameStarted, "Renderer: Frame not started");
        vkEndCommandBuffer(frames[currentFrameIndex].commandBuffer);

        VkSemaphoreSubmitInfo semInfo {};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semInfo.semaphore = frames[currentFrameIndex].imageAvailableSemaphore;
        semInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        VkCommandBufferSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        submitInfo.commandBuffer = frames[currentFrameIndex].commandBuffer;

        VkSemaphoreSubmitInfo semInfo2 {};
        semInfo2.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semInfo2.semaphore = frames[currentFrameIndex].renderFinishedSemaphore;
        semInfo2.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

        VkSubmitInfo2 submitInfo2 {};
        submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo2.waitSemaphoreInfoCount = 1;
        submitInfo2.pWaitSemaphoreInfos = &semInfo;
        submitInfo2.commandBufferInfoCount = 1;
        submitInfo2.pCommandBufferInfos = &submitInfo;
        submitInfo2.signalSemaphoreInfoCount = 1;
        submitInfo2.pSignalSemaphoreInfos = &semInfo2;

        ENGINE_VERIFY(vkQueueSubmit2(device.getGraphicsQueue(), 1, &submitInfo2, frames[currentFrameIndex].fence) == VK_SUCCESS,
            "Renderer: failed to submit command buffer submission");

        VkResult result = swapChain->presentImage(frames[currentFrameIndex].renderFinishedSemaphore, currentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapChain();
        }

        isFrameStarted = false;

        currentFrameIndex = (currentFrameIndex + 1) % Constants::MAX_FRAMES_IN_FLIGHT;
    }

    VkCommandBuffer Renderer::getCurrentCommandBuffer()
    {
        return frames[currentFrameIndex].commandBuffer;
    }

    SwapChain &Renderer::getSwapChain()
    {
        return *swapChain;
    }

    void Renderer::createFrameData()
    {
        frames.resize(Constants::MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < Constants::MAX_FRAMES_IN_FLIGHT; i++) {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = device.getGraphicsFamilyIndex();
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.pNext = nullptr;

            ENGINE_VERIFY(vkCreateCommandPool(device.getDevice(), &poolInfo, nullptr, &(frames[i].commandPool)) == VK_SUCCESS,
                "Renderer: failed to create command pool");

            VkCommandBufferAllocateInfo cmd {};
            cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmd.commandPool = frames[i].commandPool;
            cmd.commandBufferCount = 1;

            vkAllocateCommandBuffers(device.getDevice(), &cmd, &frames[i].commandBuffer);

            VkSemaphoreCreateInfo semaphoreInfo {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreInfo.flags = 0;
            semaphoreInfo.pNext = nullptr;

            ENGINE_VERIFY(vkCreateSemaphore(device.getDevice(), &semaphoreInfo, nullptr, &(frames[i].imageAvailableSemaphore)) == VK_SUCCESS,
                "Renderer: failed to create semaphore");
            ENGINE_VERIFY(vkCreateSemaphore(device.getDevice(), &semaphoreInfo, nullptr, &(frames[i].renderFinishedSemaphore)) == VK_SUCCESS,
                "Renderer: failed to create semaphore");

            VkFenceCreateInfo fenceInfo {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            fenceInfo.pNext = nullptr;

            ENGINE_VERIFY(vkCreateFence(device.getDevice(), &fenceInfo, nullptr, &(frames[i].fence)) == VK_SUCCESS,
                "Renderer: failed to create fence");
        }
    }

    void Renderer::recreateSwapChain()
    {
        VkExtent2D extent = window.GetExtent();

        while (extent.width == 0 || extent.height == 0) {
            window.PollEvents();
            extent = window.GetExtent();
        }

        vkDeviceWaitIdle(device.getDevice());

        std::unique_ptr<SwapChain> oldSwapChain = std::move(swapChain);
        swapChain = std::make_unique<SwapChain>(device, window, std::move(oldSwapChain));

        swapChainRecreatedThisFrame = true;
    }

    void Renderer::onWindowResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) {
            return;
        }
        recreateSwapChain();
    }
} // namespace Engine