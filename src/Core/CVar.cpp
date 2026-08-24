#include "CVar.h"
#include "Log.h"


namespace Engine {
    CVarBase::CVarBase(std::string_view name, std::string_view description, CVarFlags flags) : name(name), description(description), flags(flags)
    {}

    CVarSystem & CVarSystem::Get()
    {
        static CVarSystem instance;
        return instance;
    }

    void CVarSystem::Clear() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        cvars.clear();
    }

    void CVarSystem::Register(CVarBase *cvar)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::string_view name = cvar->GetName();
        std::string result(name);
        std::transform(result.begin(), result.end(), result.begin(), [](char c) {return std::tolower(c);});

        if (cvars[result] != nullptr) {
            LOG_WARN("CVar","CVar with the same name already registered");
        }

        cvars[result] = cvar;
    }

    CVarBase * CVarSystem::Find(std::string_view name)
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        std::string result(name);
        std::transform(result.begin(), result.end(), result.begin(), [](char c) {return std::tolower(c);});

        auto it = cvars.find(result);
        return it != cvars.end() ? it->second : nullptr;
    }

    std::vector<CVarBase *> CVarSystem::Search(std::string_view query)
    {
        std::shared_lock<std::shared_mutex> lock(mutex);

        std::vector<CVarBase *> result;
        std::string lowerName (query);
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](char c) {return std::tolower(c);});

        for (auto & cvar : cvars) {
            std::string_view name = cvar.first;

            size_t count = name.find(lowerName);
            if (count != std::string::npos) {
                result.push_back(cvar.second);
            }
        }

        std::sort(result.begin(),
                  result.end(),
                  [](const CVarBase* a, const CVarBase* b) {
                      return CaseInsensitive(a->GetName(), b->GetName());
                  });

        return result;
    }
}
