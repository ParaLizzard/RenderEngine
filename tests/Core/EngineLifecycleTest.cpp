#include <gtest/gtest.h>
#include <filesystem>
#include "Core/Engine.h"
#include "Core/ISubsystem.h"

namespace {
    class AutoExitSubsystem : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "AutoExitSubsystem"; }
        bool Initialize(Engine::SubsystemRegistry&) override {
            tickCount = 0;
            return true;
        }
        void Update(float) override {
            tickCount++;
            if (tickCount >= 3) {
                Engine::Engine::Get().RequestExit();
            }
        }
        void Shutdown() override {}

        int tickCount = 0;
    };
}

TEST(EngineLifecycleTest, EngineInitializationAndConfig) {
    Engine::Engine& engine = Engine::Engine::Get();
    Engine::EngineInitParams params;
    params.headless = true;
    params.configFile = "non_existent_test_config.ini";

    EXPECT_TRUE(engine.Initialize(params));
}

TEST(EngineLifecycleTest, EngineRunAndControlledExit) {
    Engine::Engine& engine = Engine::Engine::Get();
    auto& autoExit = engine.GetSubsystems().Register<AutoExitSubsystem>();

    // Engine::Run() will tick until AutoExitSubsystem requests exit on frame 3
    engine.Run();

    EXPECT_GE(autoExit.tickCount, 3);

    engine.Shutdown();

    std::filesystem::remove("non_existent_test_config.ini");
}

TEST(EngineLifecycleTest, MultiDomainFrameTiming) {
    Engine::Engine& engine = Engine::Engine::Get();
    auto& clock = engine.GetClock();
    clock.Reset();

    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), 0.0f);
    EXPECT_FLOAT_EQ(clock.GetAverageFPS(), 0.0f);

    float dt = clock.Tick();
    EXPECT_GE(dt, 0.0f);
    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), dt);
    EXPECT_FLOAT_EQ(clock.GetGameDeltaTime(), dt);

    clock.SetTimeScale(0.5f);
    EXPECT_NEAR(clock.GetGameDeltaTime(), dt * 0.5f, 1e-4f);

    clock.SetPaused(true);
    EXPECT_FLOAT_EQ(clock.GetGameDeltaTime(), 0.0f);

    clock.SetPaused(false);
    clock.SetTimeScale(1.0f);
}
