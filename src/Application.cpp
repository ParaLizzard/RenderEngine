//
// Created by Jan Varga on 25.05.2026.
//

#include "Application.h"

Engine::Application::Application()
{
}

Engine::Application::~Application()
{
}

void Engine::Application::run()
{
    while(!window.shouldClose())
    {
        glfwPollEvents();
    }
}
