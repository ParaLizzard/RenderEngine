#include "JobSubsystem.h"

#include "Core/Log.h"

namespace Engine
{
    bool JobSubsystem::Initialize(SubsystemRegistry &registry)
    {
        try {
            jobSystem = std::make_unique<JobSystem>(0);
            LOG_INFO("JobSubsystem", "Initialized JobSubsystem with {} workers", jobSystem->GetWorkerCount());

            return true;
        } catch (...) {
            LOG_FATAL("JobSubsystem", "Failed to initialize");
            return false;
        }
    }

    void JobSubsystem::Shutdown()
    {
        LOG_INFO("JobSubsystem", "Shutting down WindowSubsystem...");
        if (jobSystem) {
            jobSystem.reset();
        }
    }
} // Engine