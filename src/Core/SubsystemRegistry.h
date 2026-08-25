#pragma once
#include "Core/ISubsystem.h"
#include "Core/Assert.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <algorithm>

namespace Engine {
    class SubsystemRegistry {
    public:
        // Registers the subsystem
        template<typename T, typename... Deps, typename... Args>
        T& Register(Args&&... args) {
            static_assert(std::is_base_of_v<ISubsystem, T>, "T must derive from ISubsystem");
            auto subsystem = std::make_unique<T>(std::forward<Args>(args)...);
            T* rawPtr = subsystem.get();
            std::type_index typeIdx = std::type_index(typeid(T));

            subsystems.push_back(std::move(subsystem));
            typeMap[typeIdx] = rawPtr;
            registeredNames[typeIdx] = rawPtr->GetName();

            std::vector<std::type_index>& deps = dependencyGraph[typeIdx];
            (deps.push_back(std::type_index(typeid(Deps))), ...);

            return *rawPtr;
        }

        // Get subsystem reference
        template<typename T>
        T& Get() {
            auto it = typeMap.find(std::type_index(typeid(T)));
            ENGINE_ASSERT(it != typeMap.end(), "Subsystem '{}' not found in registry", typeid(T).name());
            return *static_cast<T*>(it->second);
        }

        // Try to get subsystem reference otherwise returns nullptr
        template<typename T>
        T* TryGet() noexcept {
            auto it = typeMap.find(std::type_index(typeid(T)));
            return it != typeMap.end() ? static_cast<T*>(it->second) : nullptr;
        }

        // Initializes every subsystem according to their dependencies
        bool InitializeAll();

        // Updates every subsystem according to their dependency
        void UpdateAll(float deltaTime);

        // Shutdowns all subsystems in reverse order according to their dependency
        void ShutdownAll();

    private:
        // Validates dependencies before initialization
        bool ValidateDependencies() const;

        std::vector<std::unique_ptr<ISubsystem>> subsystems;
        std::vector<ISubsystem*> initializationOrder;
        std::unordered_map<std::type_index, ISubsystem*> typeMap;
        std::unordered_map<std::type_index, std::vector<std::type_index>> dependencyGraph;
        std::unordered_map<std::type_index, std::string_view> registeredNames; // for diagnostic messages in ValidateDependencies
    };
}