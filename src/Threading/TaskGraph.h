#pragma once
#include "Threading/JobSystem.h"
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <atomic>

namespace Engine {
    class TaskNode {
    public:
        TaskNode(std::string name, JobFunction task, JobPriority priority)
            : name(std::move(name)), task(std::move(task)), priority(priority) {}

        void DependsOn(TaskNode& dependency) {
            dependencies.push_back(&dependency);
        }

    private:
        friend class TaskGraph;
        std::string name;
        JobFunction task;
        JobPriority priority;
        std::vector<TaskNode*> dependencies;
        std::vector<TaskNode*> dependents;
        std::atomic<uint32_t> pendingDependencies{ 0 };
    };

    class TaskGraph {
    public:
        TaskNode& AddTask(std::string name, JobFunction task, JobPriority priority = JobPriority::Normal);
        JobHandle CompileAndRun(JobSystem& jobSystem);

    private:
        static void DispatchNode(JobSystem& jobSystem, JobHandle graphHandle, TaskNode* node);
        std::vector<std::unique_ptr<TaskNode>> nodes;
    };
}