#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace Engine {
    class ConfigFile {
    public:
        bool Load(std::string_view filePath);
        bool Save(std::string_view filePath);

        std::string GetString(std::string_view section, std::string_view key, std::string_view defaultVal = "") const;
        int GetInt(std::string_view section, std::string_view key, int defaultVal = 0) const;
        float GetFloat(std::string_view section, std::string_view key, float defaultVal = 0.0f) const;
        bool GetBool(std::string_view section, std::string_view key, bool defaultVal = false) const;

        void SetString(std::string_view section, std::string_view key, std::string_view value);

        void ApplyToCVars();
        void HarvestFromCVars();

    private:
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
    };
}