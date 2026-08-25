//
// Created by Jan Varga on 25.08.2026.
//

#include "SubsystemRegistry.h"

#include <queue>

namespace Engine
{
    bool SubsystemRegistry::InitializeAll()
    {
        if (!ValidateDependencies()) {
            return false;
        }

        std::unordered_map<std::type_index, uint32_t> inDegree;
        std::unordered_map<std::type_index, std::vector<std::type_index>> adjList;
        std::queue<std::type_index> zeroIndegreeQueue;

        for (const auto& [type, _] : typeMap) {
            inDegree[type] = 0;
        }

        for (const auto &[subsystemType, deps]: dependencyGraph) {
            for (const auto &dep: deps) {
                adjList[dep].push_back(subsystemType);
                inDegree[subsystemType]++;
            }
        }

        initializationOrder.clear();
        for (auto [type, degree]: inDegree) {
            if (degree == 0) {
                zeroIndegreeQueue.push(type);
            }
        }

        while (!zeroIndegreeQueue.empty()) {
            std::type_index current = zeroIndegreeQueue.front();
            initializationOrder.push_back(typeMap[current]);
            for (auto neighbour : adjList[current]) {
                inDegree[neighbour]--;
                if (inDegree[neighbour] == 0) {
                    zeroIndegreeQueue.push(neighbour);
                }
            }
        }

        if (initializationOrder.size() != typeMap.size()) {
            LOG_FATAL("SubsystemRegistry", "Cyclic dependency detected in SubsystemRegistry!");
            return false;
        }

        for (auto subsystem : initializationOrder) {
            LOG_INFO("SubsystemRegistry", "Initializing Subsystem: {}", subsystem->GetName());
            if (!subsystem->Initialize(*this)) {
                LOG_FATAL("SubsystemRegistry", "Subsystem '{}' failed to initialize!", subsystem->GetName());
                return false;
            }
        }

        return true;
    }

    void SubsystemRegistry::UpdateAll(float deltaTime)
    {
        for (auto subsystem : initializationOrder) {
            subsystem->Update(deltaTime);
        }
    }

    void SubsystemRegistry::ShutdownAll()
    {
        for (unsigned i = initializationOrder.size(); i-- > 0;) {
            LOG_INFO("Core", "Shutting down Subsystem: {}", initializationOrder[i]->GetName() );
            initializationOrder[i]->Shutdown();
        }

        initializationOrder.clear();
        typeMap.clear();
        dependencyGraph.clear();
        registeredNames.clear();
        subsystems.clear();
    }

    bool SubsystemRegistry::ValidateDependencies() const
    {
        bool valid = true;
        for (const auto& [subsystemType, deps] : dependencyGraph) {
            for (const auto& dep : deps) {
                if (!typeMap.contains(dep)) {
                    LOG_FATAL("SubsystemRegistry",
                              "Subsystem '{}' missing dependency '{}' in registry!",
                              registeredNames.at(subsystemType),
                              dep.name());
                    valid = false;
                }
            }
        }
        return valid;
    }
} // Engine