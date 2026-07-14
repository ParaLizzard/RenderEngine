//
// Created by Jan Varga on 09.07.2026.
//

#include "AssetStreamer.h"
#include "Scene/LoaderGLTF.h"

namespace Engine
{
    AssetStreamer::AssetStreamer(JobSystem& jobSystem):jobSystem(jobSystem)
    {}

    void AssetStreamer::enqueueLoad(const std::filesystem::path &path)
    {
        pendingLoads.push_back(LoaderGLTF::loadAsync(jobSystem, path));
    }

    std::vector<ParsedGLTF> AssetStreamer::pollCompleted()
    {
        std::vector<ParsedGLTF> result;

        for (int i = pendingLoads.size() - 1; i >= 0; --i) {
            if (pendingLoads[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                ParsedGLTF parsedData = pendingLoads[i].get();
                result.push_back(parsedData);
                pendingLoads.erase(pendingLoads.begin() + i);
            }
        }

        return result;
    }
} // Engine