#include "Config.h"

namespace Engine {
    bool ConfigFile::Load(std::string_view filePath)
    {}
    bool ConfigFile::Save(std::string_view filePath)
    {}
    std::string ConfigFile::GetString(std::string_view section, std::string_view key, std::string_view defaultVal) const
    {}
    int ConfigFile::GetInt(std::string_view section, std::string_view key, int defaultVal) const
    {}
    float ConfigFile::GetFloat(std::string_view section, std::string_view key, float defaultVal) const
    {}
    bool ConfigFile::GetBool(std::string_view section, std::string_view key, bool defaultVal) const
    {}
    void ConfigFile::SetString(std::string_view section, std::string_view key, std::string_view value)
    {}
    void ConfigFile::ApplyToCVars()
    {}
    void ConfigFile::HarvestFromCVars()
    {}
} // namespace Engine


