#include "Core/Engine.h"
#include "System/Window/WindowSubsystem.h"
#include "System/Input/InputSubsystem.h"
#include "Threading/JobSubsystem.h"

int main() {
    auto& engine = Engine::Engine::Get();

    if (!engine.Initialize({ .configFile = "config/engine.ini", .headless = false })) {
        return -1;
    }

    auto& registry = engine.GetSubsystems();

    registry.Register<Engine::WindowSubsystem>(Engine::WindowProps{
        .title = "RenderEngine",
        .width = 3840,
        .height = 2160,
        .vsync = false,
        .fullscreen = false,
        .resizable = true
    });

    registry.Register<Engine::InputSubsystem, Engine::WindowSubsystem>();
    registry.Register<Engine::JobSubsystem>();

    engine.Run();

    engine.Shutdown();
    return 0;
}
