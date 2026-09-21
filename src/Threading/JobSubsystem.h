#pragma once
#include "Core/ISubsystem.h"
#include "Threading/JobSystem.h"
#include <memory>

namespace Engine {
    class JobSubsystem : public ISubsystem {
    public:
        ENGINE_NODISCARD std::string_view GetName() const override { return "JobSubsystem"; }
        bool Initialize(SubsystemRegistry& registry) override;
        void Shutdown() override;

        JobSystem& GetJobSystem() noexcept { return *jobSystem; }

    private:
        std::unique_ptr<JobSystem> jobSystem;
    };
}