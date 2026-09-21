#pragma once
#include <atomic>
#include <cstdint>
#include <limits>

#include "Core/CoreDefines.h"

namespace Engine {
    struct JobCounter {
        std::atomic<uint32_t> unfinishedJobs{ 0 };
        std::atomic<uint32_t> generation{ 0 };
    };

    class JobHandle {
    public:
        static constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

        JobHandle() = default;
        JobHandle(uint32_t index, uint32_t generation) : index(index), generation(generation) {}

        ENGINE_NODISCARD bool IsValid() const noexcept { return index != kInvalidIndex; }

        ENGINE_NODISCARD uint32_t GetIndex() const noexcept { return index; }
        ENGINE_NODISCARD uint32_t GetGeneration() const noexcept { return generation; }

    private:
        uint32_t index = kInvalidIndex;
        uint32_t generation = 0;
    };
}
