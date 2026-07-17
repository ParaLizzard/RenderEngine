#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <iostream>

#include "System/Window/Window.h"
#include "Vulkan/Device.h"
#include "Renderer/Renderer.h"
#include "Vulkan/ResourceHeap.h"
#include "Renderer/RenderGraph.h"
#include "AssetSystem/Model.h"

// Define this fixture to spin up a real Vulkan device and renderer.
// Note: This will momentarily spawn a window, as the current Engine::Device requires it.
class RenderPassIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<Engine::Window> window;
    std::unique_ptr<Engine::Device> device;
    std::unique_ptr<Engine::Renderer> renderer;
    std::unique_ptr<Engine::Model> megaBuffer;
    std::unique_ptr<Engine::ResourceHeap> resourceHeap;
    std::unique_ptr<Engine::RenderGraph> renderGraph;

    void SetUp() override {
        try {
            // Use a small 800x600 window to minimize screen flash during tests
            window = std::make_unique<Engine::Window>(800, 600, "RenderPass Integration Test");
            device = std::make_unique<Engine::Device>(*window);
            renderer = std::make_unique<Engine::Renderer>(*window, *device);
            megaBuffer = std::make_unique<Engine::Model>(*device);
            resourceHeap = std::make_unique<Engine::ResourceHeap>(*device);
            renderGraph = std::make_unique<Engine::RenderGraph>(*device);
        } catch (const std::exception& e) {
            FAIL() << "Failed to set up Vulkan context: " << e.what();
        }
    }

    void TearDown() override {
        if (device) {
            vkDeviceWaitIdle(device->getDevice());
        }
        
        // Reset in reverse order of creation
        renderGraph.reset();
        resourceHeap.reset();
        megaBuffer.reset();
        renderer.reset();
        device.reset();
        window.reset();
    }
};
