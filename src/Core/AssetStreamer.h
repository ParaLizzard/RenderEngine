#pragma once
#include <future>
#include <vector>
#include <filesystem>

namespace Engine
{
    struct ParsedGLTF;
    class JobSystem;

    class AssetStreamer
    {
    public:
        AssetStreamer(JobSystem& jobSystem);
        ~AssetStreamer() = default;

        void enqueueLoad(const std::filesystem::path& path);
        std::vector<ParsedGLTF> pollCompleted();

    private:
        JobSystem& jobSystem;
        std::vector<std::future<ParsedGLTF>> pendingLoads;
    };
}
