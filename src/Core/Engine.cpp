#include "Engine.h"

namespace Engine {
    Engine &Engine::Get()
    {
        static Engine instance;
        return instance;
    }

    bool Engine::Initialize(const EngineInitParams &params)
    {
        LOG_INFO("Engine", "Initializing RenderEngine...");


        if (config.Load(params.configFile)) {
            LOG_INFO("Engine", "Loaded config file: {}", params.configFile);
            config.ApplyToCVars();
        } else {
            LOG_WARN("Engine",
                     "Config file '{}' not found or failed to load. Using default CVar settings.",
                     params.configFile);
        }

        clock.Reset();

        LOG_INFO("Engine", "Engine core initialized successfully.");
        return true;
    }

    void Engine::Run()
    {
        if (!subsystems.InitializeAll()) return;

        running = true;

        while (running) {
            float deltaTime = clock.Tick();
            subsystems.UpdateAll(deltaTime);
        }
    }

    void Engine::RequestExit()
    {
        running = false;
    }

    void Engine::Shutdown()
    {
        LOG_INFO("Engine", "Shutting down RenderEngine...");

        subsystems.ShutdownAll();
        config.HarvestFromCVars();
        config.Save("config/engine.ini");

        LOG_INFO("Engine", "RenderEngine shutdown complete.");
    }
} // namespace Engine
