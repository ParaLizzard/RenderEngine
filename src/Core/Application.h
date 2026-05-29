#pragma once

#include "Window.h"
#include "Device.h"
#include "Scene/Camera.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/Texture.h"
#include <format>

namespace Engine
{
    class Application
    {
    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void run();
    private:
        Window window{WIDTH,HEIGHT,"Render Engine"};
        Device device{window};
        ResourceHeap resourceHeap{device};
        RenderGraph renderGraph{};
        Renderer renderer{window,device};
        Model megaBuffer{device, 500000, 1000000};
    };
}
