#include <gtest/gtest.h>
#include "Renderer/FrameInfo.h"  // Contains PerformanceMonitor

class PerformanceMonitorTest : public ::testing::Test {
protected:
    PerformanceMonitor monitor;
};

// =============================================================================
// Initial state
// =============================================================================

TEST_F(PerformanceMonitorTest, InitialFPSIsZero)
{
    EXPECT_FLOAT_EQ(monitor.GetAverageFPS(), 0.0f);
}

TEST_F(PerformanceMonitorTest, InitialFrameTimeIsZero)
{
    EXPECT_FLOAT_EQ(monitor.GetAverageFrameTime(), 0.0f);
}

// =============================================================================
// Before interval completes
// =============================================================================

TEST_F(PerformanceMonitorTest, BeforeIntervalFPSRemainsZero)
{
    // Tick 10 times at 16ms (0.16s total) — well under 2s interval
    for (int i = 0; i < 10; ++i) {
        monitor.tick(0.016f);
    }

    EXPECT_FLOAT_EQ(monitor.GetAverageFPS(), 0.0f);
    EXPECT_FLOAT_EQ(monitor.GetAverageFrameTime(), 0.0f);
}

// =============================================================================
// After first interval
// =============================================================================

TEST_F(PerformanceMonitorTest, AfterFirstIntervalCalculatesCorrectFPS)
{
    // Tick 120 times at ~16.67ms = 2.0004s (just over 2s interval)
    float dt = 1.0f / 60.0f;  // ~16.67ms
    int ticks = 0;

    // Need enough ticks so total time >= 2.0 (interval)
    while (ticks < 200) {
        monitor.tick(dt);
        ticks++;
        if (monitor.GetAverageFPS() > 0.0f) break;
    }

    float fps = monitor.GetAverageFPS();
    EXPECT_NEAR(fps, 60.0f, 1.0f)
        << "FPS should be approximately 60 after ticking at ~16.67ms intervals";
}

TEST_F(PerformanceMonitorTest, AfterFirstIntervalCalculatesCorrectFrameTime)
{
    float dt = 1.0f / 60.0f;  // ~16.67ms
    int ticks = 0;

    while (ticks < 200) {
        monitor.tick(dt);
        ticks++;
        if (monitor.GetAverageFrameTime() > 0.0f) break;
    }

    float frameTime = monitor.GetAverageFrameTime();
    EXPECT_NEAR(frameTime, 16.667f, 1.0f)
        << "Frame time should be approximately 16.67ms";
}

// =============================================================================
// 30 FPS simulation
// =============================================================================

TEST_F(PerformanceMonitorTest, ThirtyFPSSimulation)
{
    float dt = 1.0f / 30.0f;  // ~33.33ms
    int ticks = 0;

    while (ticks < 200) {
        monitor.tick(dt);
        ticks++;
        if (monitor.GetAverageFPS() > 0.0f) break;
    }

    float fps = monitor.GetAverageFPS();
    EXPECT_NEAR(fps, 30.0f, 1.0f);

    float frameTime = monitor.GetAverageFrameTime();
    EXPECT_NEAR(frameTime, 33.333f, 2.0f);
}

// =============================================================================
// Very high FPS
// =============================================================================

TEST_F(PerformanceMonitorTest, HighFPSSimulation)
{
    float dt = 1.0f / 240.0f;  // ~4.17ms
    int ticks = 0;

    while (ticks < 1000) {
        monitor.tick(dt);
        ticks++;
        if (monitor.GetAverageFPS() > 0.0f) break;
    }

    float fps = monitor.GetAverageFPS();
    EXPECT_NEAR(fps, 240.0f, 5.0f);
}

// =============================================================================
// Multiple intervals — values update
// =============================================================================

TEST_F(PerformanceMonitorTest, MultipleIntervalsUpdateValues)
{
    // First interval at 60 FPS
    float dt60 = 1.0f / 60.0f;
    for (int i = 0; i < 130; ++i) {
        monitor.tick(dt60);
    }

    float fps1 = monitor.GetAverageFPS();
    EXPECT_NEAR(fps1, 60.0f, 2.0f);

    // Second interval at 30 FPS
    float dt30 = 1.0f / 30.0f;
    for (int i = 0; i < 80; ++i) {
        monitor.tick(dt30);
    }

    float fps2 = monitor.GetAverageFPS();
    EXPECT_NEAR(fps2, 30.0f, 2.0f);
    EXPECT_NE(fps1, fps2) << "FPS should change between intervals";
}

// =============================================================================
// Zero dt does not cause issues
// =============================================================================

TEST_F(PerformanceMonitorTest, ZeroDtDoesNotCrash)
{
    for (int i = 0; i < 100; ++i) {
        EXPECT_NO_THROW(monitor.tick(0.0f));
    }

    // Timer never advances, so FPS stays at 0
    EXPECT_FLOAT_EQ(monitor.GetAverageFPS(), 0.0f);
}

// =============================================================================
// Negative dt (degenerate case) doesn't crash
// =============================================================================

TEST_F(PerformanceMonitorTest, NegativeDtDoesNotCrash)
{
    EXPECT_NO_THROW(monitor.tick(-0.016f));
    // Values may be meaningless but shouldn't crash
}

// =============================================================================
// Single tick exceeding interval
// =============================================================================

TEST_F(PerformanceMonitorTest, SingleLargeTickTriggersUpdate)
{
    monitor.tick(3.0f);  // 3 seconds — exceeds 2s interval

    float fps = monitor.GetAverageFPS();
    // 1 frame in 3 seconds ≈ 0.333 FPS
    EXPECT_GT(fps, 0.0f);
    EXPECT_LT(fps, 1.0f);

    float frameTime = monitor.GetAverageFrameTime();
    // 3000ms / 1 frame = 3000ms
    EXPECT_NEAR(frameTime, 3000.0f, 100.0f);
}
