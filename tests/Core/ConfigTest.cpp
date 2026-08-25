#include <gtest/gtest.h>
#include "Core/Config.h"
#include "Core/CVar.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace {
    AUTO_CVAR(g_ConfigTestBloom, float, "r.ConfigTest.Bloom", 0.5f, "Bloom test", CVarFlags::SaveToConfig);
    AUTO_CVAR(g_ConfigTestVSync, bool, "Window.ConfigTest.VSync", true, "VSync test", CVarFlags::SaveToConfig);
    AUTO_CVAR(g_ConfigTestReadOnly, int, "r.ConfigTest.ReadOnly", 10, "Read-only test", CVarFlags::ReadOnly);
    AUTO_CVAR(g_ConfigTestTransient, float, "r.ConfigTest.Transient", 1.0f, "Transient test", CVarFlags::None);
}

class ConfigTest : public ::testing::Test {
protected:
    std::filesystem::path tempFilePath;

    void SetUp() override {
        tempFilePath = std::filesystem::temp_directory_path() / "test_engine_config.ini";
    }

    void TearDown() override {
        if (std::filesystem::exists(tempFilePath)) {
            std::filesystem::remove(tempFilePath);
        }
    }

    void WriteTempIni(std::string_view content) {
        std::ofstream file(tempFilePath);
        file << content;
        file.close();
    }
};

// =============================================================================
// 1. Parsing & Section Extraction Tests
// =============================================================================

TEST_F(ConfigTest, LoadParsesSectionsAndKeys)
{
    std::string sampleIni = R"(
[Graphics]
r.AntiAliasing.Method = 2
r.SSAO.Enable = true
r.SSAO.Strength = 1.25

[Display]
Window.Width = 3840
Window.Height = 2160
Window.Fullscreen = false
)";

    WriteTempIni(sampleIni);

    ConfigFile config;
    EXPECT_TRUE(config.Load(tempFilePath.string()));

    EXPECT_EQ(config.GetString("Graphics", "r.AntiAliasing.Method"), "2");
    EXPECT_EQ(config.GetInt("Graphics", "r.AntiAliasing.Method"), 2);
    EXPECT_EQ(config.GetBool("Graphics", "r.SSAO.Enable"), true);
    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.SSAO.Strength"), 1.25f);

    EXPECT_EQ(config.GetInt("Display", "Window.Width"), 3840);
    EXPECT_EQ(config.GetInt("Display", "Window.Height"), 2160);
    EXPECT_FALSE(config.GetBool("Display", "Window.Fullscreen"));
}

TEST_F(ConfigTest, IgnoresCommentsAndWhitespace)
{
    std::string sampleIni = R"(
# This is a comment
; Another comment style
[Graphics] # Inline comment not matching section regex
r.Tonemap.Exposure = 2.0
; r.CommentedOut.Key = 100
# r.AnotherComment = false
)";

    WriteTempIni(sampleIni);

    ConfigFile config;
    EXPECT_TRUE(config.Load(tempFilePath.string()));

    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.Tonemap.Exposure"), 2.0f);
    EXPECT_EQ(config.GetString("Graphics", "r.CommentedOut.Key", "NOT_FOUND"), "NOT_FOUND");
    EXPECT_EQ(config.GetString("Graphics", "; r.CommentedOut.Key", "NOT_FOUND"), "NOT_FOUND");
}

// =============================================================================
// 2. Typed Getters & Fallback Defaults
// =============================================================================

TEST_F(ConfigTest, FallbacksOnMissingKeys)
{
    ConfigFile config;

    EXPECT_EQ(config.GetString("NonExistent", "Key", "DefaultStr"), "DefaultStr");
    EXPECT_EQ(config.GetInt("NonExistent", "Key", 42), 42);
    EXPECT_FLOAT_EQ(config.GetFloat("NonExistent", "Key", 3.14f), 3.14f);
    EXPECT_TRUE(config.GetBool("NonExistent", "Key", true));
    EXPECT_FALSE(config.GetBool("NonExistent", "Key", false));
}

TEST_F(ConfigTest, BoolParsingTokens)
{
    ConfigFile config;
    config.SetString("Bools", "b1", "1");
    config.SetString("Bools", "b2", "true");
    config.SetString("Bools", "b3", "on");
    config.SetString("Bools", "b4", "yes");
    config.SetString("Bools", "b5", "0");
    config.SetString("Bools", "b6", "false");
    config.SetString("Bools", "b7", "off");
    config.SetString("Bools", "b8", "no");

    EXPECT_TRUE(config.GetBool("Bools", "b1"));
    EXPECT_TRUE(config.GetBool("Bools", "b2"));
    EXPECT_TRUE(config.GetBool("Bools", "b3"));
    EXPECT_TRUE(config.GetBool("Bools", "b4"));
    EXPECT_FALSE(config.GetBool("Bools", "b5"));
    EXPECT_FALSE(config.GetBool("Bools", "b6"));
    EXPECT_FALSE(config.GetBool("Bools", "b7"));
    EXPECT_FALSE(config.GetBool("Bools", "b8"));
}

// =============================================================================
// 3. Serialization Roundtrip (Save -> Load)
// =============================================================================

TEST_F(ConfigTest, SaveAndReload)
{
    ConfigFile configOut;
    configOut.SetString("Shadows", "r.Shadow.CSM.PCSS", "1");
    configOut.SetString("Shadows", "r.Shadow.Cascade0Samples", "16");
    configOut.SetString("Audio", "Volume.Master", "0.85");

    EXPECT_TRUE(configOut.Save(tempFilePath.string()));

    ConfigFile configIn;
    EXPECT_TRUE(configIn.Load(tempFilePath.string()));

    EXPECT_EQ(configIn.GetInt("Shadows", "r.Shadow.CSM.PCSS"), 1);
    EXPECT_EQ(configIn.GetInt("Shadows", "r.Shadow.Cascade0Samples"), 16);
    EXPECT_FLOAT_EQ(configIn.GetFloat("Audio", "Volume.Master"), 0.85f);
}

// =============================================================================
// 4. Two-Tier CVar Synchronization Tests
// =============================================================================

TEST_F(ConfigTest, ApplyToCVarsSync)
{
    ConfigFile config;
    config.SetString("Graphics", "r.ConfigTest.Bloom", "2.75");
    config.SetString("Display", "Window.ConfigTest.VSync", "false");
    config.SetString("Graphics", "r.ConfigTest.ReadOnly", "999"); // ReadOnly should be ignored

    config.ApplyToCVars();

    EXPECT_FLOAT_EQ(g_ConfigTestBloom.Get(), 2.75f);
    EXPECT_FALSE(g_ConfigTestVSync.Get());
    EXPECT_EQ(g_ConfigTestReadOnly.Get(), 10); // Unchanged
}

TEST_F(ConfigTest, HarvestFromCVarsSync)
{
    g_ConfigTestBloom.Set(4.5f);
    g_ConfigTestVSync.Set(true);
    g_ConfigTestTransient.Set(99.0f); // Transient (no SaveToConfig)

    ConfigFile config;
    config.HarvestFromCVars();

    // Bloom (SaveToConfig, prefix "r.") should be in "Graphics"
    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.ConfigTest.Bloom"), 4.5f);

    // VSync (SaveToConfig, prefix "Window.") should be in "Display"
    EXPECT_TRUE(config.GetBool("Display", "Window.ConfigTest.VSync"));

    // Transient should NOT be harvested
    EXPECT_EQ(config.GetString("Graphics", "r.ConfigTest.Transient", "NONE"), "NONE");
}
