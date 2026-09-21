#include <gtest/gtest.h>
#include "Threading/JobSystem.h"
#include "Threading/TaskGraph.h"
#include "Threading/JobSubsystem.h"
#include "Core/SubsystemRegistry.h"
#include <vector>
#include <atomic>
#include <numeric>
#include <string>
#include <mutex>

using namespace Engine;

// Test Scenario 1: Basic Fire & Wait Verification
TEST(JobSystemTest, ExecutesSingleTaskToCompletion) {
    JobSystem jobSystem(4);
    std::atomic<bool> executed{ false };

    JobHandle handle = jobSystem.Execute([&executed]() {
        executed.store(true, std::memory_order_release);
    });

    jobSystem.Wait(handle);
    EXPECT_TRUE(executed.load(std::memory_order_acquire));
    EXPECT_TRUE(jobSystem.IsCompleted(handle));
}

// Test Scenario 2: ParallelFor Stress Test (100,000 Elements)
TEST(JobSystemTest, ParallelForVisitsAllElementsWithoutRaces) {
    JobSystem jobSystem(8);
    constexpr uint32_t kItemCount = 100'000;
    std::vector<uint32_t> data(kItemCount, 0);

    JobHandle handle = jobSystem.Dispatch(kItemCount, 512, [&data](uint32_t start, uint32_t end) {
        for (uint32_t i = start; i < end; ++i) {
            data[i] = 1;
        }
    });

    jobSystem.Wait(handle);

    for (uint32_t i = 0; i < kItemCount; ++i) {
        ASSERT_EQ(data[i], 1u) << "Index " << i << " was not processed!";
    }
}

// Test Scenario 3: Work Helping Deadlock Prevention
TEST(JobSystemTest, WorkerThreadWaitingOnChildJobsHelpsWithoutDeadlock) {
    // Single worker thread: if worker blocks OS thread, child task can never run!
    JobSystem jobSystem(1);

    std::atomic<bool> childExecuted{ false };

    JobHandle parentHandle = jobSystem.Execute([&]() {
        JobHandle childHandle = jobSystem.Execute([&]() {
            childExecuted.store(true, std::memory_order_release);
        });
        // Worker calls wait on child: must HELP rather than block!
        jobSystem.Wait(childHandle);
    });

    jobSystem.Wait(parentHandle);
    EXPECT_TRUE(childExecuted.load(std::memory_order_acquire));
}

// Test Scenario 4: Diamond TaskGraph Dependency Order
TEST(JobSystemTest, TaskGraphExecutesInStrictTopologicalOrder) {
    JobSystem jobSystem(4);
    TaskGraph graph;

    std::string trace = "";
    std::mutex traceMutex;

    auto appendTrace = [&](char c) {
        std::lock_guard<std::mutex> lock(traceMutex);
        trace += c;
    };

    auto& taskA = graph.AddTask("TaskA", [&]() { appendTrace('A'); });
    auto& taskB = graph.AddTask("TaskB", [&]() { appendTrace('B'); });
    auto& taskC = graph.AddTask("TaskC", [&]() { appendTrace('C'); });
    auto& taskD = graph.AddTask("TaskD", [&]() { appendTrace('D'); });

    taskB.DependsOn(taskA);
    taskC.DependsOn(taskA);
    taskD.DependsOn(taskB);
    taskD.DependsOn(taskC);

    JobHandle handle = graph.CompileAndRun(jobSystem);
    jobSystem.Wait(handle);

    ASSERT_EQ(trace.length(), 4u);
    EXPECT_EQ(trace.front(), 'A');
    EXPECT_EQ(trace.back(), 'D');
}

// Test Scenario 5: Priority Ordering & Multiple Dispatches
TEST(JobSystemTest, HighFrequencySubmissionStress) {
    JobSystem jobSystem(4);
    constexpr uint32_t kIterations = 1000;
    std::atomic<uint32_t> counter{ 0 };

    std::vector<JobHandle> handles;
    handles.reserve(kIterations);

    for (uint32_t i = 0; i < kIterations; ++i) {
        JobPriority p = (i % 3 == 0) ? JobPriority::Critical : (i % 3 == 1 ? JobPriority::Normal : JobPriority::Background);
        handles.push_back(jobSystem.Execute([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }, p));
    }

    for (const auto& h : handles) {
        jobSystem.Wait(h);
    }

    EXPECT_EQ(counter.load(), kIterations);
}

// Test Scenario 6: JobSubsystem Lifecycle Verification
TEST(JobSystemTest, JobSubsystemLifecycle) {
    JobSubsystem subsystem;
    EXPECT_EQ(subsystem.GetName(), "JobSubsystem");

    SubsystemRegistry registry;
    EXPECT_TRUE(subsystem.Initialize(registry));
    EXPECT_GT(subsystem.GetJobSystem().GetWorkerCount(), 0u);

    subsystem.Shutdown();
}
