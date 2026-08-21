#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "Core/Clock.h"

class ClockTest : public ::testing::Test {
protected:
    Engine::Clock clock;
};

// =============================================================================
// 1. Initialization and Reset
// =============================================================================

TEST_F(ClockTest, DefaultInitialization)
{
    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), 0.0f);
    EXPECT_FLOAT_EQ(clock.GetDeltaTimeMs(), 0.0f);
    EXPECT_DOUBLE_EQ(clock.GetTotalTime(), 0.0);
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), 0.0);
    EXPECT_EQ(clock.GetFrameCount(), 0u);
    EXPECT_FLOAT_EQ(clock.GetAverageFPS(), 0.0f);
    EXPECT_FLOAT_EQ(clock.GetTimeScale(), 1.0f);
    EXPECT_FALSE(clock.IsPaused());
    EXPECT_NEAR(clock.GetFixedDeltaTime(), 1.0f / 60.0f, 1e-5f);
    EXPECT_NEAR(clock.GetFixedFPS(), 60.0f, 1e-3f);
    EXPECT_FLOAT_EQ(clock.GetMaxDeltaTime(), 0.25f);
    EXPECT_NEAR(clock.GetMinFPS(), 4.0f, 1e-3f);
}

TEST_F(ClockTest, ResetRestoresInitialState)
{
    // Advance clock
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.Tick();
    clock.SetTimeScale(2.0f);
    clock.SetPaused(true);
    clock.Step(5);

    EXPECT_GT(clock.GetFrameCount(), 0u);
    EXPECT_GT(clock.GetTotalTime(), 0.0);

    clock.Reset();

    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), 0.0f);
    EXPECT_FLOAT_EQ(clock.GetDeltaTimeMs(), 0.0f);
    EXPECT_DOUBLE_EQ(clock.GetTotalTime(), 0.0);
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), 0.0);
    EXPECT_EQ(clock.GetFrameCount(), 0u);
    EXPECT_FLOAT_EQ(clock.GetAverageFPS(), 0.0f);
}

// =============================================================================
// 2. Tick and Delta Time
// =============================================================================

TEST_F(ClockTest, TickAdvancesTimeAndFrames)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    float dt = clock.Tick();

    EXPECT_GT(dt, 0.0f);
    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), dt);
    EXPECT_NEAR(clock.GetDeltaTimeMs(), dt * 1000.0f, 1e-3f);
    EXPECT_DOUBLE_EQ(clock.GetTotalTime(), static_cast<double>(dt));
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), static_cast<double>(dt));
    EXPECT_EQ(clock.GetFrameCount(), 1u);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    float dt2 = clock.Tick();

    EXPECT_GT(dt2, 0.0f);
    EXPECT_EQ(clock.GetFrameCount(), 2u);
    EXPECT_DOUBLE_EQ(clock.GetTotalTime(), static_cast<double>(dt) + static_cast<double>(dt2));
}

// =============================================================================
// 3. Time Scaling and Pause
// =============================================================================

TEST_F(ClockTest, TimeScalingAffectsGameDeltaTime)
{
    clock.SetTimeScale(2.0f);
    EXPECT_FLOAT_EQ(clock.GetTimeScale(), 2.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    float dt = clock.Tick();

    EXPECT_NEAR(clock.GetGameDeltaTime(), dt * 2.0f, 1e-4f);
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), static_cast<double>(dt * 2.0f));

    // Negative timescale should clamp to 0.0f
    clock.SetTimeScale(-0.5f);
    EXPECT_FLOAT_EQ(clock.GetTimeScale(), 0.0f);
}

TEST_F(ClockTest, PausingHaltsGameTime)
{
    clock.SetPaused(true);
    EXPECT_TRUE(clock.IsPaused());

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    float dt = clock.Tick();

    EXPECT_GT(dt, 0.0f);
    EXPECT_FLOAT_EQ(clock.GetDeltaTime(), dt);
    EXPECT_FLOAT_EQ(clock.GetGameDeltaTime(), 0.0f);
    EXPECT_DOUBLE_EQ(clock.GetTotalTime(), static_cast<double>(dt));
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), 0.0);

    // Unpause resumes game time
    clock.SetPaused(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    float dt2 = clock.Tick();

    EXPECT_FLOAT_EQ(clock.GetGameDeltaTime(), dt2);
    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), static_cast<double>(dt2));
}

// =============================================================================
// 4. Frame Stepping
// =============================================================================

TEST_F(ClockTest, FrameSteppingWhenPaused)
{
    clock.SetPaused(true);

    // Step 1 frame
    clock.Step(1);

    // During this tick, game delta time should advance by fixed timestep
    float expectedStepTime = clock.GetFixedDeltaTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.Tick();

    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), static_cast<double>(expectedStepTime));

    // Subsequent tick without stepping should not advance game time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.Tick();

    EXPECT_DOUBLE_EQ(clock.GetGameTotalTime(), static_cast<double>(expectedStepTime));
    EXPECT_FLOAT_EQ(clock.GetGameDeltaTime(), 0.0f);
}

// =============================================================================
// 5. Delta Time Clamping and Minimum FPS Floor
// =============================================================================

TEST_F(ClockTest, MaxDeltaTimeClampsSpikes)
{
    // Set max delta time to 50ms (20 FPS min)
    clock.SetMinFPS(20.0f);
    EXPECT_NEAR(clock.GetMaxDeltaTime(), 0.05f, 1e-4f);
    EXPECT_NEAR(clock.GetMinFPS(), 20.0f, 1e-3f);

    // Sleep longer than maxDeltaTime (e.g. 80ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    float dt = clock.Tick();

    EXPECT_LE(dt, 0.05001f);
    EXPECT_FLOAT_EQ(dt, clock.GetMaxDeltaTime());
}

TEST_F(ClockTest, FixedFPSConfiguration)
{
    clock.SetFixedFPS(120.0f);
    EXPECT_NEAR(clock.GetFixedDeltaTime(), 1.0f / 120.0f, 1e-5f);
    EXPECT_NEAR(clock.GetFixedFPS(), 120.0f, 1e-3f);

    clock.SetFixedDeltaTime(1.0f / 30.0f);
    EXPECT_NEAR(clock.GetFixedFPS(), 30.0f, 1e-3f);
}

// =============================================================================
// 6. Average FPS and Frametime Accessors
// =============================================================================

TEST_F(ClockTest, FrametimeHelperCalculations)
{
    EXPECT_FLOAT_EQ(clock.GetAverageFrametimeMs(), 0.0f);
}
