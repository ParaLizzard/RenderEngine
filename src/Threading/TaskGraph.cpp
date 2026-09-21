#include "TaskGraph.h"

namespace Engine
{
    TaskNode & TaskGraph::AddTask(std::string name, JobFunction task, JobPriority priority)
    {
        auto node = std::make_unique<TaskNode>(std::move(name), std::move(task), priority);
        TaskNode* ptr = node.get();
        nodes.push_back(std::move(node));
        return *ptr;
    }

    JobHandle TaskGraph::CompileAndRun(JobSystem &jobSystem)
    {
        if (nodes.empty()) return {};

        for (auto& node : nodes) {
            node->dependents.clear();
        }

        for (auto& node : nodes) {
            for (auto* dep : node->dependencies) {
                dep->dependents.push_back(node.get());
            }
            node->pendingDependencies.store(static_cast<uint32_t>(node->dependencies.size()), std::memory_order_relaxed);
        }

        JobHandle handle = jobSystem.AcquireCounter(static_cast<uint32_t>(nodes.size()));

        for (auto& node : nodes) {
            if (node->pendingDependencies.load(std::memory_order_relaxed) == 0) {
                DispatchNode(jobSystem, handle, node.get());
            }
        }

        return handle;
    }

    void TaskGraph::DispatchNode(JobSystem &jobSystem, JobHandle graphHandle, TaskNode *node)
    {
        jobSystem.Execute([&jobSystem, graphHandle, node]() {
            if (node->task) {
                node->task();
            }

            jobSystem.FinishJob(graphHandle.GetIndex());

            for (TaskNode* dep : node->dependents) {
                if (dep->pendingDependencies.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    DispatchNode(jobSystem, graphHandle, dep);
                }
            }
        }, node->priority);
    }
} // Engine