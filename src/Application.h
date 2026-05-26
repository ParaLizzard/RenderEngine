#pragma once

#include "Window.h"
#include "Device.h"
#include "ResourceHeap.h"

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
    };
}
