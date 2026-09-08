#include <gtest/gtest.h>
#include "Renderer/RenderSettings.h"
#include "Core/Config.h"

using namespace Engine;

class RenderSettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Record defaults and ensure clean baseline
        CVarSSAOEnabled.ResetToDefault();
        CVarSSAOStrength.ResetToDefault();
        CVarSSAORadius.ResetToDefault();
        CVarAAMethod.ResetToDefault();
        CVarTonemapMethod.ResetToDefault();
        CVarTonemapExposure.ResetToDefault();
        CVarFreezeCulling.ResetToDefault();
        CVarHiZDebugMode.ResetToDefault();
        CVarHiZDebugMip.ResetToDefault();
    }

    void TearDown() override {
        // Reset all back to defaults
        CVarSSAOEnabled.ResetToDefault();
        CVarSSAOStrength.ResetToDefault();
        CVarSSAORadius.ResetToDefault();
        CVarAAMethod.ResetToDefault();
        CVarTonemapMethod.ResetToDefault();
        CVarTonemapExposure.ResetToDefault();
        CVarFreezeCulling.ResetToDefault();
        CVarHiZDebugMode.ResetToDefault();
        CVarHiZDebugMip.ResetToDefault();
    }
};

// =============================================================================
// 1. CVar Declaration & Registration Tests (§3.3)
// =============================================================================

TEST_F(RenderSettingsTest, StandardCVarsRegistrationAndDefaults)
{
    auto& cvarSys = CVarSystem::Get();

    // 1. r.SSAO.Enable
    CVarBase* ssaoEnable = cvarSys.Find("r.SSAO.Enable");
    ASSERT_NE(ssaoEnable, nullptr);
    EXPECT_EQ(ssaoEnable->GetTypeName(), "bool");
    EXPECT_TRUE(CVarSSAOEnabled.Get());
    EXPECT_TRUE(ssaoEnable->HasFlag(CVarFlags::SaveToConfig));
    EXPECT_TRUE(ssaoEnable->HasFlag(CVarFlags::RenderDirty));

    // 2. r.SSAO.Strength
    CVarBase* ssaoStrength = cvarSys.Find("r.SSAO.Strength");
    ASSERT_NE(ssaoStrength, nullptr);
    EXPECT_EQ(ssaoStrength->GetTypeName(), "float");
    EXPECT_FLOAT_EQ(CVarSSAOStrength.Get(), 1.25f);
    EXPECT_TRUE(ssaoStrength->HasFlag(CVarFlags::SaveToConfig));
    EXPECT_FALSE(ssaoStrength->HasFlag(CVarFlags::RenderDirty));

    // 3. r.SSAO.Radius
    CVarBase* ssaoRadius = cvarSys.Find("r.SSAO.Radius");
    ASSERT_NE(ssaoRadius, nullptr);
    EXPECT_EQ(ssaoRadius->GetTypeName(), "float");
    EXPECT_FLOAT_EQ(CVarSSAORadius.Get(), 0.5f);
    EXPECT_TRUE(ssaoRadius->HasFlag(CVarFlags::SaveToConfig));

    // 4. r.AntiAliasing.Method
    CVarBase* aaMethod = cvarSys.Find("r.AntiAliasing.Method");
    ASSERT_NE(aaMethod, nullptr);
    EXPECT_EQ(aaMethod->GetTypeName(), "int");
    EXPECT_EQ(CVarAAMethod.Get(), 2);
    EXPECT_TRUE(aaMethod->HasFlag(CVarFlags::SaveToConfig));
    EXPECT_TRUE(aaMethod->HasFlag(CVarFlags::RenderDirty));

    // 5. r.Tonemap.Method
    CVarBase* tonemapMethod = cvarSys.Find("r.Tonemap.Method");
    ASSERT_NE(tonemapMethod, nullptr);
    EXPECT_EQ(tonemapMethod->GetTypeName(), "int");
    EXPECT_EQ(CVarTonemapMethod.Get(), 1);
    EXPECT_TRUE(tonemapMethod->HasFlag(CVarFlags::SaveToConfig));

    // 6. r.Tonemap.Exposure
    CVarBase* tonemapExposure = cvarSys.Find("r.Tonemap.Exposure");
    ASSERT_NE(tonemapExposure, nullptr);
    EXPECT_EQ(tonemapExposure->GetTypeName(), "float");
    EXPECT_FLOAT_EQ(CVarTonemapExposure.Get(), 2.0f);
    EXPECT_TRUE(tonemapExposure->HasFlag(CVarFlags::SaveToConfig));

    // 7. r.Debug.FreezeCulling
    CVarBase* freezeCulling = cvarSys.Find("r.Debug.FreezeCulling");
    ASSERT_NE(freezeCulling, nullptr);
    EXPECT_EQ(freezeCulling->GetTypeName(), "bool");
    EXPECT_FALSE(CVarFreezeCulling.Get());
    EXPECT_FALSE(freezeCulling->HasFlag(CVarFlags::SaveToConfig));

    // 8. r.Debug.HiZView
    CVarBase* hizView = cvarSys.Find("r.Debug.HiZView");
    ASSERT_NE(hizView, nullptr);
    EXPECT_EQ(hizView->GetTypeName(), "int");
    EXPECT_EQ(CVarHiZDebugMode.Get(), 0);
    EXPECT_FALSE(hizView->HasFlag(CVarFlags::SaveToConfig));

    // 9. r.Debug.HiZMipLevel
    CVarBase* hizMip = cvarSys.Find("r.Debug.HiZMipLevel");
    ASSERT_NE(hizMip, nullptr);
    EXPECT_EQ(hizMip->GetTypeName(), "int");
    EXPECT_EQ(CVarHiZDebugMip.Get(), 0);
    EXPECT_FALSE(hizMip->HasFlag(CVarFlags::SaveToConfig));
}

// =============================================================================
// 2. Reactive OnChanged Callback Tests (§3.3)
// =============================================================================

TEST_F(RenderSettingsTest, ReactiveDirtyCallbacks)
{
    bool aaCallbackFired = false;
    int aaOld = -1, aaNew = -1;

    auto tokenAA = CVarAAMethod.OnChanged([&](int oldVal, int newVal) {
        aaCallbackFired = true;
        aaOld = oldVal;
        aaNew = newVal;
    });

    bool ssaoCallbackFired = false;
    bool ssaoOld = true, ssaoNew = true;

    auto tokenSSAO = CVarSSAOEnabled.OnChanged([&](bool oldVal, bool newVal) {
        ssaoCallbackFired = true;
        ssaoOld = oldVal;
        ssaoNew = newVal;
    });

    // Trigger AA change (2 -> 1)
    CVarAAMethod.Set(1);
    EXPECT_TRUE(aaCallbackFired);
    EXPECT_EQ(aaOld, 2);
    EXPECT_EQ(aaNew, 1);

    // Trigger SSAO change (true -> false)
    CVarSSAOEnabled.Set(false);
    EXPECT_TRUE(ssaoCallbackFired);
    EXPECT_TRUE(ssaoOld);
    EXPECT_FALSE(ssaoNew);

    // Cleanup callbacks
    CVarAAMethod.RemoveCallback(tokenAA);
    CVarSSAOEnabled.RemoveCallback(tokenSSAO);

    // Further change should not trigger removed callbacks
    aaCallbackFired = false;
    CVarAAMethod.Set(0);
    EXPECT_FALSE(aaCallbackFired);
}

// =============================================================================
// 3. Two-Tier Config Synchronization Tests (§3.3)
// =============================================================================

TEST_F(RenderSettingsTest, ConfigHarvestAndApplySync)
{
    // Modify some SaveToConfig values
    CVarSSAOStrength.Set(2.5f);
    CVarSSAORadius.Set(0.8f);
    CVarTonemapExposure.Set(3.5f);
    CVarAAMethod.Set(1);

    ConfigFile config;
    config.HarvestFromCVars();

    // Standard graphics CVars should be harvested into [Graphics]
    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.SSAO.Strength"), 2.5f);
    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.SSAO.Radius"), 0.8f);
    EXPECT_FLOAT_EQ(config.GetFloat("Graphics", "r.Tonemap.Exposure"), 3.5f);
    EXPECT_EQ(config.GetInt("Graphics", "r.AntiAliasing.Method"), 1);

    // Now test ApplyToCVars from config
    config.SetString("Graphics", "r.SSAO.Strength", "1.75");
    config.SetString("Graphics", "r.Tonemap.Exposure", "1.5");
    config.SetString("Graphics", "r.SSAO.Enable", "false");
    config.ApplyToCVars();

    EXPECT_FLOAT_EQ(CVarSSAOStrength.Get(), 1.75f);
    EXPECT_FLOAT_EQ(CVarTonemapExposure.Get(), 1.5f);
    EXPECT_FALSE(CVarSSAOEnabled.Get());
}
