#include "WindowSubsystem.h"
#include "Core/Engine.h"
#include "Core/SubsystemRegistry.h"
#include "System/Events/Event.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/WindowEvents.h"

#if defined(_WIN32) && !defined(ENGINE_USE_GLFW)
    #include "WindowWin32.h"
#else
    #include "WindowGLFW.h"
#endif

namespace Engine {
    WindowSubsystem::WindowSubsystem(const WindowProps &props)
    {
        properties = props;
        if (properties.width <= 0 || properties.height <= 0) {
            properties.height = 720;
            properties.width = 1280;
        }

        window = nullptr;
    }

    bool WindowSubsystem::Initialize(SubsystemRegistry &registry)
    {
        #if defined(_WIN32) && !defined(ENGINE_USE_GLFW)
                window = std::make_unique<WindowWin32>(properties);
        #elif defined(ENGINE_USE_GLFW) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
                window = std::make_unique<WindowGLFW>(properties);
        #else
                LOG_FATAL("Window", "Unsupported platform for WindowSubsystem!");
                return false;
        #endif


        if (!window || !window->GetNativeHandle()) {
            LOG_FATAL("Window", "Failed to create native window!");
            return false;
        }

        LOG_INFO("Window",
                 "Created Window: Title: {}, Width: {}, Height: {}, Aspect: {}, Vsync: {}, fullscreen: {} ",
                 properties.title,
                 window->GetWidth(),
                 window->GetHeight(),
                 window->GetAspectRatio(),
                 properties.vsync,
                 properties.fullscreen);

        return true;
    }

    void WindowSubsystem::Update(float deltaTime)
    {
        ISubsystem::Update(deltaTime);
        ENGINE_ASSERT(window != nullptr, "WindowSubsystem::Update called with uninitialized window");

        window->PollEvents();

        if (window->ShouldClose()) {
            WindowCloseEvent event;
            EventDispatcher::Get().PostEvent(event);
            Engine::Get().RequestExit();
        }
    }

    void WindowSubsystem::Shutdown()
    {
        LOG_INFO("Window", "Shutting down WindowSubsystem...");
        window.reset();


    }
}
