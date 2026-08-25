#pragma once
#include <string_view>

#include "CoreDefines.h"

namespace Engine {
    class SubsystemRegistry;

    class ISubsystem {
    public:
        virtual ~ISubsystem() = default;

        ENGINE_NODISCARD virtual std::string_view GetName() const = 0;
        virtual bool Initialize(SubsystemRegistry& registry) = 0;
        virtual void Update(float deltaTime) {}
        virtual void Shutdown() = 0;
    };
}
