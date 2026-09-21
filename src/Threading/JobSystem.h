#pragma once
#include "Core/ISubsystem.h"
#include "Threading/JobHandle.h"
#include "Threading/WorkStealingQueue.h"
#include <functional>
#include <vector>
#include <thread>
#include <string_view>
#include <span>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <array>

namespace Engine {
    static thread_local std::optional<uint32_t> currentWorkerIndex = std::nullopt;

    enum class JobPriority : uint8_t {
        Critical = 0,
        Normal   = 1,
        Background = 2,
        Count
    };

    using JobFunction = std::function<void()>;
    using ParallelForFunction = std::function<void(uint32_t startIndex, uint32_t endIndex)>;

    struct InternalJob {
        JobFunction function;
        uint32_t counterIndex = JobHandle::kInvalidIndex;
    };

    class TaskGraph;

    class JobSystem {
        friend class TaskGraph;
    public:
        explicit JobSystem(uint32_t threadCount = 0);
        ~JobSystem();

        JobHandle Execute(JobFunction task, JobPriority priority = JobPriority::Normal);
        JobHandle Dispatch(uint32_t totalItemCount, uint32_t groupSize, ParallelForFunction func, JobPriority priority = JobPriority::Normal);

        void Wait(const JobHandle& handle);

        ENGINE_NODISCARD bool IsCompleted(const JobHandle& handle) const noexcept;
        ENGINE_NODISCARD uint32_t GetWorkerCount() const noexcept { return static_cast<uint32_t>(workers.size()); }

    private:
        void WorkerLoop(uint32_t threadIndex);
        bool TryExecuteLocalOrSteal(uint32_t threadIndex);
        void FinishJob(uint32_t counterIndex);
        JobHandle AcquireCounter(uint32_t initialCount = 1);
        void ReleaseCounter(uint32_t index);


        std::vector<std::thread> workers;
        std::vector<std::array<WorkStealingQueue<InternalJob>, static_cast<size_t>(JobPriority::Count)>> queues;

        static constexpr size_t MAX_COUNTERS = 4096;
        std::vector<JobCounter> counterPool;
        std::vector<uint32_t> freeCounterIndices;
        std::mutex counterPoolMutex;
        std::condition_variable nonWorkerWaitCV;
        std::atomic<bool> running{ true };
    };
}