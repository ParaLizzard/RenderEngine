#pragma once
#include <future>
#include <vector>
#include <filesystem>

namespace Engine
{
    struct ParsedGLTF;
    class JobSystemOld;

    class AssetStreamer
    {
    public:
        AssetStreamer(JobSystemOld& jobSystem);
        ~AssetStreamer() = default;

        void enqueueLoad(const std::filesystem::path& path);
        std::vector<ParsedGLTF> pollCompleted();

    private:
        JobSystemOld& jobSystem;
        std::vector<std::future<ParsedGLTF>> pendingLoads;
    };
}
