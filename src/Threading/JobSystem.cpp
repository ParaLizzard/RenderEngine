#include "JobSystem.h"

#include <ranges>
#include <xmmintrin.h>
#include "Core/Assert.h"
#include "Core/Log.h"

namespace Engine
{
    JobSystem::JobSystem(uint32_t threadCount):
    queues((threadCount > 0) ? threadCount : std::max(1u, std::thread::hardware_concurrency() - 1)),
    counterPool(MAX_COUNTERS)
    {
        threadCount = (threadCount > 0) ? threadCount : std::max(1u, std::thread::hardware_concurrency() - 1);

        auto r = std::views::iota(0u, static_cast<uint32_t>(MAX_COUNTERS));
        freeCounterIndices.assign(r.begin(), r.end());

        workers.reserve(threadCount);
        for(uint32_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this, i]() {WorkerLoop(i);});
        }
    }

    JobSystem::~JobSystem()
    {
        running.store(false, std::memory_order_release);
        nonWorkerWaitCV.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    JobHandle JobSystem::Execute(JobFunction task, JobPriority priority)
    {
        JobHandle handle = AcquireCounter(1);

        InternalJob job {std::move(task), handle.GetIndex()};

        bool bPushed = false;
        if (currentWorkerIndex.has_value()) {
            bPushed = queues[*currentWorkerIndex][static_cast<uint32_t>(priority)].Push(job);
        } else {
            static std::atomic<uint32_t> s_NonWorkerIndex{ 0 };
            uint32_t targetWorker = s_NonWorkerIndex.fetch_add(1, std::memory_order_relaxed) % queues.size();
            bPushed = queues[targetWorker][static_cast<uint32_t>(priority)].Push(job);
        }

        if (!bPushed) {
            try {
                if (job.function) job.function();
            } catch (const std::exception& e) {
                LOG_ERROR("JobSystem", "Job exception: {}", e.what());
            } catch (...) {
                LOG_ERROR("JobSystem", "Unknown job exception");
            }
            FinishJob(job.counterIndex);
        }

        return handle;
    }

    JobHandle JobSystem::Dispatch(
        uint32_t totalItemCount,
        uint32_t groupSize,
        ParallelForFunction func,
        JobPriority priority)
    {
        if (totalItemCount == 0 || groupSize == 0) return {};

        uint32_t numOfGroups = (totalItemCount + groupSize - 1) / groupSize;

        JobHandle handle = AcquireCounter(numOfGroups);

        for (uint32_t i = 0; i < numOfGroups; i++) {
            uint32_t startIndex = i * groupSize;
            uint32_t endIndex = std::min(startIndex + groupSize, totalItemCount);

            InternalJob job {[func, startIndex ,endIndex](){func(startIndex,endIndex);}, handle.GetIndex()};

            uint32_t targetWorker = (currentWorkerIndex.value_or(0) + i) % queues.size();

            bool bPushed = queues[targetWorker][static_cast<uint32_t>(priority)].Push(job);

            if (!bPushed) {
                try {
                    if (job.function) job.function();
                } catch (const std::exception& e) {
                    LOG_ERROR("JobSystem", "Job exception: {}", e.what());
                } catch (...) {
                    LOG_ERROR("JobSystem", "Unknown job exception");
                }
                FinishJob(job.counterIndex);
            }
        }

        return handle;
    }

    void JobSystem::Wait(const JobHandle &handle)
    {
        if (!handle.IsValid()) return;

        if (currentWorkerIndex.has_value()) {
            while (!IsCompleted(handle)) {
                if (!TryExecuteLocalOrSteal(*currentWorkerIndex)) {
                    std::this_thread::yield();
                }
            }
        } else {
            std::unique_lock<std::mutex> lock(counterPoolMutex);
            nonWorkerWaitCV.wait(lock, [this, &handle]() {
                return IsCompleted(handle) || !running.load(std::memory_order_relaxed);
            });
        }

        ReleaseCounter(handle.GetIndex());
    }

    bool JobSystem::IsCompleted(const JobHandle &handle) const noexcept
    {
        if (!handle.IsValid()) return true;

        if (counterPool[handle.GetIndex()].generation.load(std::memory_order_acquire) != handle.GetGeneration()) return true;
        if (counterPool[handle.GetIndex()].unfinishedJobs.load(std::memory_order_acquire) == 0) return true;

        return false;
    }

    void JobSystem::WorkerLoop(uint32_t threadIndex)
    {
        currentWorkerIndex = threadIndex;
        uint32_t idleCounter = 0;

        while (running.load(std::memory_order_relaxed))
        {
            bool bSuccess = TryExecuteLocalOrSteal(threadIndex);

            if (bSuccess) {
                idleCounter = 0;
            } else {
                ++idleCounter;

                if (idleCounter < 32) {
                    #if defined(_MSC_VER) || defined(__x86_64__) || defined(_M_X64)
                                        _mm_pause();
                    #else
                                        std::this_thread::yield();
                    #endif
                }
                else if (idleCounter < 64)
                {
                    std::this_thread::yield();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }

        }

        while (TryExecuteLocalOrSteal(threadIndex)) {}
    }

    bool JobSystem::TryExecuteLocalOrSteal(uint32_t threadIndex)
    {
        auto runJob = [this](InternalJob& job) {
            try {
                if (job.function) {
                    job.function();
                }
            } catch (const std::exception& e) {
                LOG_ERROR("JobSystem", "Job exception: {}", e.what());
            } catch (...) {
                LOG_ERROR("JobSystem", "Unknown job exception");
            }
            FinishJob(job.counterIndex);
        };

        for (uint32_t i = 0; i < static_cast<uint32_t>(JobPriority::Count); i++) {
            std::optional<InternalJob> job = queues[threadIndex][i].Pop();

            if (job == std::nullopt) continue;

            runJob(job.value());
            return true;
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(JobPriority::Count); i++) {
            for (uint32_t j = 1; j < queues.size(); j++) {
                uint32_t targetWorker = (threadIndex + j) % queues.size();
                std::optional<InternalJob> job = queues[targetWorker][i].Steal();

                if (job == std::nullopt) continue;

                runJob(job.value());
                return true;
            }
        }

        return false;
    }

    void JobSystem::FinishJob(uint32_t counterIndex)
    {
        if (counterIndex == JobHandle::kInvalidIndex) return;

        uint32_t prev = counterPool[counterIndex].unfinishedJobs.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1) {
            std::lock_guard<std::mutex> lock(counterPoolMutex);
            nonWorkerWaitCV.notify_all();
        }
    }

    JobHandle JobSystem::AcquireCounter(uint32_t initialCount)
    {
        std::lock_guard<std::mutex> lock(counterPoolMutex);

        ENGINE_ASSERT(!freeCounterIndices.empty(), "Out of free counter indices");

        uint32_t counterIndex = freeCounterIndices.back();
        freeCounterIndices.pop_back();

        counterPool[counterIndex].unfinishedJobs.store(initialCount, std::memory_order_release);
        uint32_t gen = counterPool[counterIndex].generation.load(std::memory_order_relaxed);

        return JobHandle(counterIndex, gen);
    }

    void JobSystem::ReleaseCounter(uint32_t index)
    {
        std::lock_guard<std::mutex> lock(counterPoolMutex);

        counterPool[index].generation.fetch_add(1, std::memory_order_release);
        freeCounterIndices.push_back(index);
    }
} // Engine