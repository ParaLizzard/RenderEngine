#include "Core/Engine.h"
#include "System/Window/WindowSubsystem.h"
#include "System/Input/InputSubsystem.h"

int main() {
    auto& engine = Engine::Engine::Get();

    // 1. Initialize coordinator (loads config/engine.ini, sets up CVars and Clock)
    if (!engine.Initialize({ .configFile = "config/engine.ini", .headless = false })) {
        return -1;
    }

    // 2. Register Subsystems
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

    // 3. Run Main Loop
    engine.Run();

    // 4. Clean Shutdown
    engine.Shutdown();
    return 0;
}
