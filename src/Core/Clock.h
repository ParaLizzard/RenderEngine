#pragma once
#include <chrono>

#include "CoreDefines.h"

namespace Engine {
    class Clock {
    public:
        Clock();

        void Reset();
        float Tick();

        // Gets frame time of current frame (in seconds)
        ENGINE_NODISCARD float GetDeltaTime() const noexcept { return deltaTime; }

        // Gets frame time of current frame (in milliseconds)
        ENGINE_NODISCARD float GetDeltaTimeMs() const noexcept { return deltaTime * 1000.0f; }

        // Gets average frame time (in milliseconds)
        ENGINE_NODISCARD float GetAverageFrametimeMs() const noexcept { return averageFps > 0.0f ? (1000.0f / averageFps) : 0.0f;}

        // Gets game delta time (can be paused, timescaled, or single-stepped)
        ENGINE_NODISCARD float GetGameDeltaTime() const noexcept
        {
            if (!paused) {
                return deltaTime * timeScale;
            }
            if (stepFramesRemaining > 0) {
                return fixedDeltaTime * timeScale;
            }
            return 0.0f;
        }


        // Gets fixed simulation timestep (in seconds)
        ENGINE_NODISCARD float GetFixedDeltaTime() const noexcept { return fixedDeltaTime; }

        // Sets fixed simulation timestep in seconds            
        void SetFixedDeltaTime(float dt) noexcept { fixedDeltaTime = dt > 0.0f ? dt : (1.0f / 60.0f); }

        // Sets fixed simulation rate in FPS (e.g. 60.0f -> 1/60s)
        void SetFixedFPS(float fixedFps) noexcept { fixedDeltaTime = fixedFps > 0.0f ? (1.0f / fixedFps) : (1.0f / 60.0f); }

        // Gets fixed simulation rate in FPS
        ENGINE_NODISCARD float GetFixedFPS() const noexcept { return fixedDeltaTime > 0.0f ? (1.0f / fixedDeltaTime) : 0.0f; }

        // Sets minimum FPS threshold to clamp delta time spikes (e.g. 4.0f -> max 0.25s frame time)
        void SetMinFPS(float minFps) noexcept { maxDeltaTime = minFps > 0.0f ? (1.0f / minFps) : 0.25f; }

        // Gets minimum FPS threshold
        ENGINE_NODISCARD float GetMinFPS() const noexcept { return maxDeltaTime > 0.0f ? (1.0f / maxDeltaTime) : 0.0f; }

        // Sets maximum frame time directly in seconds (to prevent large spikes)
        void SetMaxDeltaTime(float maxDt) noexcept { maxDeltaTime = maxDt > 0.0f ? maxDt : 0.25f; }

        // Gets maximum frame time in seconds
        ENGINE_NODISCARD float GetMaxDeltaTime() const noexcept { return maxDeltaTime; }

        // Gets total time since start of rendering (in seconds)
        ENGINE_NODISCARD double GetTotalTime() const noexcept { return totalTime; }

        // Total game time of running game (in seconds)
        ENGINE_NODISCARD double GetGameTotalTime() const noexcept { return gameTotalTime; }

        // Gets total frame count since start of rendering
        ENGINE_NODISCARD uint64_t GetFrameCount() const noexcept { return frameCount; }

        // Gets average frames per second
        ENGINE_NODISCARD float GetAverageFPS() const noexcept { return averageFps; }

        // Sets timescale of the game time
        void SetTimeScale(float scale) noexcept { timeScale = scale > 0.0f ? scale : 0.0f; }

        // Gets timescale of the game time
        ENGINE_NODISCARD float GetTimeScale() const noexcept { return timeScale; }

        // Pauses game time
        void SetPaused(bool isPaused) noexcept { paused = isPaused; }

        // Game time is paused/unpaused
        ENGINE_NODISCARD bool IsPaused() const noexcept { return paused; }
        
        // Advances the clock by exactly N frames, ignoring real time
        void Step(uint32_t frames = 1) noexcept { stepFramesRemaining += frames; }

    private:
        using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
        TimePoint startTime;
        TimePoint lastFrameTime;
        float deltaTime = 0.0f;
        float fixedDeltaTime = 1.0f / 60.0f;
        float maxDeltaTime = 0.25f; // Clamps to min 4 FPS (0.25s) to prevent simulation spikes
        float timeScale = 1.0f;
        bool paused = false;
        double totalTime = 0.0;
        double gameTotalTime = 0.0;
        uint64_t frameCount = 0;
        float averageFps = 0.0f;
        float fpsAccumulator = 0.0f;
        uint32_t fpsFrameCounter = 0;
        uint32_t stepFramesRemaining = 0;
    };
}
