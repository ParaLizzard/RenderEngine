#include "Application.h"

using namespace Engine;

Application::Application()
{
}

Application::~Application()
{
}

void Application::run()
{
    while(!window.shouldClose())
    {
        glfwPollEvents();
    }
}
