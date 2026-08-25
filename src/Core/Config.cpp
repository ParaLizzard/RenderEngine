#include "Config.h"
#include "CVar.h"
#include <regex>
#include <format>
#include <fstream>
#include <charconv>

namespace Engine {

    bool ConfigFile::Load(std::string_view filePath)
    {
        sections.clear();

        std::string pathStr(filePath);
        std::ifstream file(pathStr);
        if (!file.is_open()) return false;

        std::regex section_regex(R"(^\s*\[\s*([^\]]+?)\s*\]\s*$)");
        std::regex key_value_regex(R"(^\s*([^=]+?)\s*=\s*(.*?)\s*$)");

        std::smatch match;
        std::string line;
        std::string section;

        while (std::getline(file, line)) {
            size_t commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }

            if (std::regex_match(line, match, section_regex)) {
                section = match[1].str();
            }
            else if (std::regex_match(line, match, key_value_regex)) {
                sections[section][match[1].str()] = match[2].str();
            }
        }

        return true;
    }

    bool ConfigFile::Save(std::string_view filePath)
    {
        std::string pathStr(filePath);
        std::ofstream file(pathStr);
        if (!file.is_open()) return false;

        for (const auto& [sectionName, keyValues] : sections) {
            if (keyValues.empty()) continue;

            if (!sectionName.empty()) {
                file << std::format("[{}]\n", sectionName);
            }
            for (const auto& [key, value] : keyValues) {
                file << std::format("{} = {}\n", key, value);
            }
            file << "\n";
        }

        return true;
    }

    std::string ConfigFile::GetString(std::string_view section, std::string_view key, std::string_view defaultVal) const
    {
        auto secIt = sections.find(std::string(section));
        if (secIt != sections.end()) {
            auto keyIt = secIt->second.find(std::string(key));
            if (keyIt != secIt->second.end()) {
                return keyIt->second;
            }
        }
        return std::string(defaultVal);
    }

    int ConfigFile::GetInt(std::string_view section, std::string_view key, int defaultVal) const
    {
        std::string val = GetString(section, key);
        if (val.empty()) return defaultVal;

        int result = defaultVal;
        auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
        return (ec == std::errc{}) ? result : defaultVal;
    }

    float ConfigFile::GetFloat(std::string_view section, std::string_view key, float defaultVal) const
    {
        std::string val = GetString(section, key);
        if (val.empty()) return defaultVal;

        try {
            return std::stof(val);
        } catch (...) {
            return defaultVal;
        }
    }

    bool ConfigFile::GetBool(std::string_view section, std::string_view key, bool defaultVal) const
    {
        std::string val = GetString(section, key);
        if (val.empty()) return defaultVal;

        if (val == "1" || val == "true" || val == "True" || val == "TRUE" || val == "on" || val == "ON" || val == "yes") {
            return true;
        }
        if (val == "0" || val == "false" || val == "False" || val == "FALSE" || val == "off" || val == "OFF"|| val == "no") {
            return false;
        }

        return defaultVal;
    }

    void ConfigFile::SetString(std::string_view section, std::string_view key, std::string_view value)
    {
        sections[std::string(section)][std::string(key)] = std::string(value);
    }

    void ConfigFile::ApplyToCVars()
    {
        for (const auto& [sectionName, keyValues] : sections) {
            if (keyValues.empty()) continue;

            for (const auto& [key, value] : keyValues) {
                CVarBase* base = CVarSystem::Get().Find(key);
                if (base && !base->HasFlag(CVarFlags::ReadOnly)) {
                    base->FromString(value);
                } else {
                    LOG_WARN("Config", "Unknown or read-only key '{}' in config file, skipped", key);
                }
            }
        }
    }

    void ConfigFile::HarvestFromCVars()
    {
        auto& cvarSystem = CVarSystem::Get();
        for (const auto& [_, cvar] : cvarSystem.GetAll()) {
            if (!cvar || !cvar->HasFlag(CVarFlags::SaveToConfig)) {
                continue;
            }

            std::string cvarName(cvar->GetName());

            std::string targetSection;
            for (const auto& [secName, keyValues] : sections) {
                if (keyValues.find(cvarName) != keyValues.end()) {
                    targetSection = secName;
                    break;
                }
            }

            if (targetSection.empty()) {
                if (cvarName.starts_with("r.Shadow.")) {
                    targetSection = "Shadows";
                } else if (cvarName.starts_with("r.")) {
                    targetSection = "Graphics";
                } else if (cvarName.starts_with("Window.") || cvarName.starts_with("sys.")) {
                    targetSection = "Display";
                } else {
                    targetSection = "General";
                }
            }

            sections[targetSection][cvarName] = cvar->ToString();
        }
    }


} // namespace Engine
