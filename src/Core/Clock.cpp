#include "Clock.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
    Clock::Clock()
    {
        Reset();
    }

    void Clock::Reset()
    {
        startTime = std::chrono::steady_clock::now();
        lastFrameTime = startTime;
        deltaTime = 0.0f;
        totalTime = 0.0;
        gameTotalTime = 0.0;
        frameCount = 0;
        averageFps = 0.0f;
        fpsAccumulator = 0.0f;
        fpsFrameCounter = 0;
        stepFramesRemaining = 0;
    }

    float Clock::Tick()
    {
        const auto currentTime = std::chrono::steady_clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        if (std::isnan(deltaTime) || std::isinf(deltaTime) || deltaTime < 0.0f) {
           deltaTime = 0.0f;
        }

        if (deltaTime > maxDeltaTime) {
            deltaTime = maxDeltaTime;
        }

        totalTime += deltaTime;
        gameTotalTime += GetGameDeltaTime();
        frameCount++;

        if (paused && stepFramesRemaining > 0) {
            stepFramesRemaining--;
        }

        fpsAccumulator += deltaTime;
        fpsFrameCounter++;

        if (fpsAccumulator >= 1.0f) {
            averageFps = static_cast<float>(fpsFrameCounter) / fpsAccumulator;
            fpsAccumulator = 0.0f;
            fpsFrameCounter = 0;
        }

        return deltaTime;
    }
} // Engine