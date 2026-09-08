#include <gtest/gtest.h>
#include "Core/CVar.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace Engine;

// Declare sample global CVars using AUTO_CVAR for macro testing
AUTO_CVAR(g_TestAutoBool, bool, "test.auto.bool", true, "Test auto-registered bool CVar", CVarFlags::None);
AUTO_CVAR(g_TestAutoFloat, float, "test.auto.float", 3.14f, "Test auto-registered float CVar", CVarFlags::SaveToConfig);
AUTO_CVAR(g_TestAutoInt, int32_t, "test.auto.int", 42, "Test auto-registered int CVar", CVarFlags::RenderDirty);

class CVarTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset or prepare clean state if needed
    }

    void TearDown() override {
        // Note: Global AUTO_CVARs remain in CVarSystem singleton,
        // but local test CVars can be cleared or overridden.
    }
};

// =============================================================================
// 1. Core Specification Tests (From Engine Refactor Design Doc §1.6)
// =============================================================================

TEST_F(CVarTest, CVarTypeSafetyAndReflection)
{
    // Register CVar<float> and CVar<bool>
    CVar<float> floatVar("r.test.FloatVal", 1.5f, "Test float parameter", CVarFlags::None);
    CVar<bool> boolVar("r.test.BoolVal", true, "Test bool parameter", CVarFlags::None);

    // Initial values
    EXPECT_FLOAT_EQ(floatVar.Get(), 1.5f);
    EXPECT_TRUE(boolVar.Get());

    // Reflection Type Names
    EXPECT_EQ(floatVar.GetTypeName(), "float");
    EXPECT_EQ(boolVar.GetTypeName(), "bool");

    // Set() and Get()
    floatVar.Set(4.25f);
    boolVar.Set(false);
    EXPECT_FLOAT_EQ(floatVar.Get(), 4.25f);
    EXPECT_FALSE(boolVar.Get());

    // ToString()
    EXPECT_EQ(boolVar.ToString(), "false");
    boolVar.Set(true);
    EXPECT_EQ(boolVar.ToString(), "true");

    // FromString() on bool
    EXPECT_TRUE(boolVar.FromString("0"));
    EXPECT_FALSE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("1"));
    EXPECT_TRUE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("false"));
    EXPECT_FALSE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("true"));
    EXPECT_TRUE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("off"));
    EXPECT_FALSE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("on"));
    EXPECT_TRUE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("no"));
    EXPECT_FALSE(boolVar.Get());
    EXPECT_TRUE(boolVar.FromString("yes"));
    EXPECT_TRUE(boolVar.Get());
    EXPECT_FALSE(boolVar.FromString("invalid_bool"));
    EXPECT_TRUE(boolVar.Get()); // Unchanged on failure

    // FromString() on float
    EXPECT_TRUE(floatVar.FromString("10.5"));
    EXPECT_FLOAT_EQ(floatVar.Get(), 10.5f);
    EXPECT_TRUE(floatVar.FromString("-2.75"));
    EXPECT_FLOAT_EQ(floatVar.Get(), -2.75f);
    EXPECT_FALSE(floatVar.FromString("not_a_number"));
    EXPECT_FLOAT_EQ(floatVar.Get(), -2.75f); // Unchanged on failure
    EXPECT_FALSE(floatVar.FromString("10.5abc")); // Trailing invalid chars rejected
}

TEST_F(CVarTest, MultiSubscriberTokenLifecycle)
{
    CVar<int32_t> testVar("r.test.MultiSub", 0, "Subscriber lifecycle test", CVarFlags::None);

    int callCount1 = 0;
    int callCount2 = 0;
    int callCount3 = 0;

    int lastOldVal1 = 0, lastNewVal1 = 0;
    int lastOldVal2 = 0, lastNewVal2 = 0;
    int lastOldVal3 = 0, lastNewVal3 = 0;

    // Register 3 callbacks
    auto token1 = testVar.OnChanged([&](const int32_t& oldVal, const int32_t& newVal) {
        callCount1++;
        lastOldVal1 = oldVal;
        lastNewVal1 = newVal;
    });

    auto token2 = testVar.OnChanged([&](const int32_t& oldVal, const int32_t& newVal) {
        callCount2++;
        lastOldVal2 = oldVal;
        lastNewVal2 = newVal;
    });

    auto token3 = testVar.OnChanged([&](const int32_t& oldVal, const int32_t& newVal) {
        callCount3++;
        lastOldVal3 = oldVal;
        lastNewVal3 = newVal;
    });

    // Ensure tokens are unique and non-zero
    EXPECT_NE(token1, 0u);
    EXPECT_NE(token2, 0u);
    EXPECT_NE(token3, 0u);
    EXPECT_NE(token1, token2);
    EXPECT_NE(token2, token3);

    // Modify value - verify all 3 execute
    testVar.Set(10);
    EXPECT_EQ(callCount1, 1);
    EXPECT_EQ(callCount2, 1);
    EXPECT_EQ(callCount3, 1);
    EXPECT_EQ(lastOldVal1, 0);
    EXPECT_EQ(lastNewVal1, 10);
    EXPECT_EQ(lastOldVal2, 0);
    EXPECT_EQ(lastNewVal2, 10);
    EXPECT_EQ(lastOldVal3, 0);
    EXPECT_EQ(lastNewVal3, 10);

    // Remove callback #2 using SubscriptionToken
    testVar.RemoveCallback(token2);

    // Modify value again - verify only #1 and #3 execute
    testVar.Set(25);
    EXPECT_EQ(callCount1, 2);
    EXPECT_EQ(callCount2, 1); // Callback #2 stayed at 1
    EXPECT_EQ(callCount3, 2);
    EXPECT_EQ(lastOldVal1, 10);
    EXPECT_EQ(lastNewVal1, 25);
    EXPECT_EQ(lastOldVal3, 10);
    EXPECT_EQ(lastNewVal3, 25);

    // Setting same value again should be a no-op (no callbacks fired)
    testVar.Set(25);
    EXPECT_EQ(callCount1, 2);
    EXPECT_EQ(callCount2, 1);
    EXPECT_EQ(callCount3, 2);

    // Remove remaining callbacks
    testVar.RemoveCallback(token1);
    testVar.RemoveCallback(token3);

    testVar.Set(50);
    EXPECT_EQ(callCount1, 2);
    EXPECT_EQ(callCount2, 1);
    EXPECT_EQ(callCount3, 2);
}

TEST_F(CVarTest, CVarSystemSearch)
{
    CVarSystem& system = CVarSystem::Get();

    CVar<bool> searchEnable("test.Search.Enable", true, "Enable search test", CVarFlags::SaveToConfig);
    CVar<float> searchStrength("test.Search.Strength", 1.2f, "Search strength multiplier", CVarFlags::SaveToConfig);
    CVar<float> searchRadius("test.Search.Radius", 0.5f, "Search sampling radius", CVarFlags::SaveToConfig);
    CVar<int32_t> otherMethod("test.Other.Method", 2, "Other Method", CVarFlags::SaveToConfig);

    system.Register(&searchEnable);
    system.Register(&searchStrength);
    system.Register(&searchRadius);
    system.Register(&otherMethod);

    // Search for prefix "test.Search"
    std::vector<CVarBase*> results = system.Search("test.Search");

    EXPECT_EQ(results.size(), 3u);

    // Verify all returned items match "test.Search"
    for (CVarBase* cvar : results) {
        EXPECT_NE(cvar, nullptr);
        std::string name(cvar->GetName());
        EXPECT_NE(name.find("test.Search"), std::string::npos);
    }

    // Verify search is case-insensitive
    std::vector<CVarBase*> resultsLower = system.Search("test.search");
    EXPECT_EQ(resultsLower.size(), 3u);

    // Verify sorting order (case-insensitive alphabetical)
    // "test.Search.Enable", "test.Search.Radius", "test.Search.Strength"
    ASSERT_GE(results.size(), 3u);
    EXPECT_EQ(results[0]->GetName(), "test.Search.Enable");
    EXPECT_EQ(results[1]->GetName(), "test.Search.Radius");
    EXPECT_EQ(results[2]->GetName(), "test.Search.Strength");
}

// =============================================================================
// 2. Default Value and Modification State
// =============================================================================

TEST_F(CVarTest, DefaultValueAndReset)
{
    CVar<float> cvar("test.reset", 5.0f, "Reset test", CVarFlags::None);

    EXPECT_FLOAT_EQ(cvar.Get(), 5.0f);
    EXPECT_FLOAT_EQ(cvar.GetDefault(), 5.0f);
    EXPECT_FALSE(cvar.IsModified());

    cvar.Set(12.5f);
    EXPECT_FLOAT_EQ(cvar.Get(), 12.5f);
    EXPECT_FLOAT_EQ(cvar.GetDefault(), 5.0f);
    EXPECT_TRUE(cvar.IsModified());

    cvar.ResetToDefault();
    EXPECT_FLOAT_EQ(cvar.Get(), 5.0f);
    EXPECT_FALSE(cvar.IsModified());
}

// =============================================================================
// 3. Min/Max Range Clamping
// =============================================================================

TEST_F(CVarTest, MinMaxClamping)
{
    CVar<float> clampedFloat("test.clamped.float", 5.0f, "Clamped float", CVarFlags::None, 0.0f, 10.0f);

    clampedFloat.Set(-5.0f);
    EXPECT_FLOAT_EQ(clampedFloat.Get(), 0.0f);

    clampedFloat.Set(15.0f);
    EXPECT_FLOAT_EQ(clampedFloat.Get(), 10.0f);

    clampedFloat.Set(7.5f);
    EXPECT_FLOAT_EQ(clampedFloat.Get(), 7.5f);

    CVar<int32_t> clampedInt("test.clamped.int", 50, "Clamped int", CVarFlags::None, 10, 100);

    clampedInt.Set(5);
    EXPECT_EQ(clampedInt.Get(), 10);

    clampedInt.Set(200);
    EXPECT_EQ(clampedInt.Get(), 100);

    clampedInt.Set(42);
    EXPECT_EQ(clampedInt.Get(), 42);
}

// =============================================================================
// 4. Flags and ReadOnly Behavior
// =============================================================================

TEST_F(CVarTest, FlagsAndReadOnly)
{
    CVarFlags combinedFlags = CVarFlags::SaveToConfig | CVarFlags::RenderDirty;
    CVar<int32_t> flagVar("test.flags", 1, "Flag test", combinedFlags);

    EXPECT_TRUE(flagVar.HasFlag(CVarFlags::SaveToConfig));
    EXPECT_TRUE(flagVar.HasFlag(CVarFlags::RenderDirty));
    EXPECT_FALSE(flagVar.HasFlag(CVarFlags::ReadOnly));
    EXPECT_FALSE(flagVar.HasFlag(CVarFlags::RequiresRestart));
    EXPECT_EQ(flagVar.GetFlags(), combinedFlags);

    // ReadOnly CVar cannot be modified by Set or FromString
    CVar<int32_t> readOnlyVar("test.readonly", 100, "ReadOnly test", CVarFlags::ReadOnly);
    EXPECT_TRUE(readOnlyVar.HasFlag(CVarFlags::ReadOnly));
    EXPECT_EQ(readOnlyVar.Get(), 100);

    readOnlyVar.Set(200);
    EXPECT_EQ(readOnlyVar.Get(), 100);

    EXPECT_FALSE(readOnlyVar.FromString("200"));
    EXPECT_EQ(readOnlyVar.Get(), 100);
}

// =============================================================================
// 5. String Type CVar Support
// =============================================================================

TEST_F(CVarTest, StringCVar)
{
    CVar<std::string> strVar("r.test.ShaderPath", "shaders/default.vert", "Shader file path", CVarFlags::SaveToConfig);

    EXPECT_EQ(strVar.GetTypeName(), "string");
    EXPECT_EQ(strVar.Get(), "shaders/default.vert");
    EXPECT_EQ(strVar.ToString(), "shaders/default.vert");
    EXPECT_FALSE(strVar.IsModified());

    strVar.Set("shaders/custom.vert");
    EXPECT_EQ(strVar.Get(), "shaders/custom.vert");
    EXPECT_TRUE(strVar.IsModified());

    EXPECT_TRUE(strVar.FromString("shaders/postprocess.frag"));
    EXPECT_EQ(strVar.Get(), "shaders/postprocess.frag");

    strVar.ResetToDefault();
    EXPECT_EQ(strVar.Get(), "shaders/default.vert");
    EXPECT_FALSE(strVar.IsModified());
}

// =============================================================================
// 6. CVarSystem Find and FindExact
// =============================================================================

TEST_F(CVarTest, SystemFindAndFindExact)
{
    CVarSystem& system = CVarSystem::Get();

    CVar<float> exposure("test.Find.Exposure", 2.0f, "Exposure value", CVarFlags::None);
    system.Register(&exposure);

    // Case-insensitive Find
    CVarBase* foundBase1 = system.Find("test.Find.Exposure");
    CVarBase* foundBase2 = system.Find("test.find.exposure");
    CVarBase* foundBase3 = system.Find("TEST.FIND.EXPOSURE");

    ASSERT_NE(foundBase1, nullptr);
    EXPECT_EQ(foundBase1, &exposure);
    EXPECT_EQ(foundBase2, &exposure);
    EXPECT_EQ(foundBase3, &exposure);

    // Find non-existent
    EXPECT_EQ(system.Find("test.NonExistent.Param"), nullptr);

    // FindExact with correct type
    CVar<float>* typedExposure = system.FindExact<float>("test.find.exposure");
    ASSERT_NE(typedExposure, nullptr);
    EXPECT_FLOAT_EQ(typedExposure->Get(), 2.0f);

    // FindExact with mismatched type returns nullptr
    CVar<int32_t>* mismatchedInt = system.FindExact<int32_t>("test.find.exposure");
    EXPECT_EQ(mismatchedInt, nullptr);

    CVar<bool>* mismatchedBool = system.FindExact<bool>("test.find.exposure");
    EXPECT_EQ(mismatchedBool, nullptr);
}

// =============================================================================
// 7. AUTO_CVAR Macro Global Registration
// =============================================================================

TEST_F(CVarTest, AutoCVarRegistration)
{
    CVarSystem& system = CVarSystem::Get();

    // Verify auto-registered CVars exist in the central registry
    CVar<bool>* autoBool = system.FindExact<bool>("test.auto.bool");
    CVar<float>* autoFloat = system.FindExact<float>("test.auto.float");
    CVar<int32_t>* autoInt = system.FindExact<int32_t>("test.auto.int");

    ASSERT_NE(autoBool, nullptr);
    ASSERT_NE(autoFloat, nullptr);
    ASSERT_NE(autoInt, nullptr);

    EXPECT_TRUE(autoBool->Get());
    EXPECT_FLOAT_EQ(autoFloat->Get(), 3.14f);
    EXPECT_EQ(autoInt->Get(), 42);

    EXPECT_TRUE(autoFloat->HasFlag(CVarFlags::SaveToConfig));
    EXPECT_TRUE(autoInt->HasFlag(CVarFlags::RenderDirty));
}

// =============================================================================
// 8. Concurrency & Deadlock Prevention
// =============================================================================

TEST_F(CVarTest, MultiThreadedReadWriteConcurrency)
{
    CVar<int32_t> sharedVar("test.concurrent", 0, "Concurrency test", CVarFlags::None);
    std::atomic<bool> stopRunning = false;
    std::atomic<uint64_t> readSuccessCount = 0;

    constexpr int NUM_READERS = 4;
    constexpr int NUM_WRITERS = 2;
    constexpr int WRITE_ITERATIONS = 1000;

    std::vector<std::thread> readerThreads;
    for (int i = 0; i < NUM_READERS; ++i) {
        readerThreads.emplace_back([&]() {
            while (!stopRunning.load(std::memory_order_relaxed)) {
                int val = sharedVar.Get();
                (void)val;
                readSuccessCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> writerThreads;
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writerThreads.emplace_back([&, i]() {
            for (int j = 0; j < WRITE_ITERATIONS; ++j) {
                sharedVar.Set(i * 10000 + j);
            }
        });
    }

    for (auto& t : writerThreads) {
        t.join();
    }
    stopRunning.store(true, std::memory_order_relaxed);
    for (auto& t : readerThreads) {
        t.join();
    }

    EXPECT_GT(readSuccessCount.load(), 0u);
}

TEST_F(CVarTest, DeadlockPreventionInCallbacks)
{
    // Subsystem callbacks must be able to query or mutate other CVars without deadlocking
    CVar<int32_t> varA("test.deadlock.A", 0, "Deadlock test A", CVarFlags::None);
    CVar<int32_t> varB("test.deadlock.B", 100, "Deadlock test B", CVarFlags::None);

    varA.OnChanged([&](const int32_t& oldVal, const int32_t& newVal) {
        (void)oldVal;
        // Read another CVar from inside the callback
        int bVal = varB.Get();
        // Mutate another CVar from inside the callback
        varB.Set(bVal + newVal);
    });

    varA.Set(10);

    EXPECT_EQ(varA.Get(), 10);
    EXPECT_EQ(varB.Get(), 110);
}
