#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <regex>

namespace Engine {
    class ConfigFile {
    public:
        // Loads .ini file to internal structures
        bool Load(std::string_view filePath);

        // Saves .ini config file
        bool Save(std::string_view filePath);

        // Gets string value of config section and key
        std::string GetString(std::string_view section, std::string_view key, std::string_view defaultVal = "") const;

        // Gets integer value of config section and key
        int GetInt(std::string_view section, std::string_view key, int defaultVal = 0) const;

        // Gets float value of config section and key
        float GetFloat(std::string_view section, std::string_view key, float defaultVal = 0.0f) const;

        // Gets bool value of config section and key
        bool GetBool(std::string_view section, std::string_view key, bool defaultVal = false) const;

        // Sets value of config section and key
        void SetString(std::string_view section, std::string_view key, std::string_view value);

        // Applies values to existing CVars
        void ApplyToCVars();

        // Gets values from CVars and saves them to config
        void HarvestFromCVars();

    private:
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
    };
}