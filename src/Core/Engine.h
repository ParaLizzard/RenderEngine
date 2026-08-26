#pragma once
#include "Core/SubsystemRegistry.h"
#include "Core/Clock.h"
#include "Core/Config.h"

namespace Engine {
    struct EngineInitParams {
        std::string configFile = "config/engine.ini";
        bool headless = false;
    };

    class Engine {
    public:
        // Get class instance
        static Engine& Get();

        // Initialize the engine
        bool Initialize(const EngineInitParams& params = {});

        // Run all the subsystems and the engine
        void Run();

        // Request exit from the application
        void RequestExit();

        // Shutdown all subsystems and the engine
        void Shutdown();

        // Get engine subsystems
        SubsystemRegistry& GetSubsystems() noexcept { return subsystems; }

        // Get engine clock
        const Clock& GetClock() const noexcept { return clock; }

        // Get engine config
        ConfigFile& GetConfig() noexcept { return config; }

    private:
        Engine() = default;

        bool running = false;
        Clock clock;
        ConfigFile config;
        SubsystemRegistry subsystems;
    };
}