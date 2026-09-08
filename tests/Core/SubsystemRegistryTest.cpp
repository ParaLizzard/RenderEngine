#include <gtest/gtest.h>
#include "Core/SubsystemRegistry.h"
#include "Core/ISubsystem.h"
#include <vector>
#include <string>

namespace {
    static std::vector<std::string> executionLog;

    class SubsystemA : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "SubsystemA"; }
        bool Initialize(Engine::SubsystemRegistry&) override {
            executionLog.push_back("Init_A");
            return true;
        }
        void Update(float) override { executionLog.push_back("Update_A"); }
        void Shutdown() override { executionLog.push_back("Shutdown_A"); }
    };

    class SubsystemB : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "SubsystemB"; }
        bool Initialize(Engine::SubsystemRegistry&) override {
            executionLog.push_back("Init_B");
            return true;
        }
        void Update(float) override { executionLog.push_back("Update_B"); }
        void Shutdown() override { executionLog.push_back("Shutdown_B"); }
    };

    class SubsystemC : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "SubsystemC"; }
        bool Initialize(Engine::SubsystemRegistry&) override {
            executionLog.push_back("Init_C");
            return true;
        }
        void Update(float) override { executionLog.push_back("Update_C"); }
        void Shutdown() override { executionLog.push_back("Shutdown_C"); }
    };

    class SubsystemD : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "SubsystemD"; }
        bool Initialize(Engine::SubsystemRegistry&) override {
            executionLog.push_back("Init_D");
            return true;
        }
        void Update(float) override { executionLog.push_back("Update_D"); }
        void Shutdown() override { executionLog.push_back("Shutdown_D"); }
    };

    class DependentOnMissing : public Engine::ISubsystem {
    public:
        std::string_view GetName() const override { return "DependentOnMissing"; }
        bool Initialize(Engine::SubsystemRegistry&) override { return true; }
        void Update(float) override {}
        void Shutdown() override {}
    };
}

class SubsystemRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        executionLog.clear();
    }
};

TEST_F(SubsystemRegistryTest, TopologicalSortDiamondDependency) {
    Engine::SubsystemRegistry registry;

    // Register in arbitrary order: D depends on B,C; B depends on A; C depends on A; A has no deps
    registry.Register<SubsystemD, SubsystemB, SubsystemC>();
    registry.Register<SubsystemB, SubsystemA>();
    registry.Register<SubsystemC, SubsystemA>();
    registry.Register<SubsystemA>();

    EXPECT_TRUE(registry.InitializeAll());

    // A must be initialized first, D must be initialized last
    ASSERT_GE(executionLog.size(), 4u);
    EXPECT_EQ(executionLog[0], "Init_A");
    EXPECT_EQ(executionLog[3], "Init_D");

    // Test UpdateAll
    executionLog.clear();
    registry.UpdateAll(0.016f);
    EXPECT_EQ(executionLog.size(), 4u);
    EXPECT_EQ(executionLog[0], "Update_A");
    EXPECT_EQ(executionLog[3], "Update_D");

    // Test ShutdownAll (must be reverse order: D first, A last)
    executionLog.clear();
    registry.ShutdownAll();
    ASSERT_EQ(executionLog.size(), 4u);
    EXPECT_EQ(executionLog[0], "Shutdown_D");
    EXPECT_EQ(executionLog[3], "Shutdown_A");
}

TEST_F(SubsystemRegistryTest, MissingDependencyValidation) {
    Engine::SubsystemRegistry registry;
    // Register DependentOnMissing with SubsystemA as dependency, but do NOT register SubsystemA
    registry.Register<DependentOnMissing, SubsystemA>();

    EXPECT_FALSE(registry.InitializeAll());
}

TEST_F(SubsystemRegistryTest, SubsystemRetrievalAndTryGet) {
    Engine::SubsystemRegistry registry;
    registry.Register<SubsystemA>();

    EXPECT_TRUE(registry.InitializeAll());

    EXPECT_TRUE(registry.Has<SubsystemA>());
    EXPECT_FALSE(registry.Has<SubsystemB>());

    SubsystemA& subA = registry.Get<SubsystemA>();
    EXPECT_EQ(subA.GetName(), "SubsystemA");

    SubsystemA* trySubA = registry.TryGet<SubsystemA>();
    ASSERT_NE(trySubA, nullptr);
    EXPECT_EQ(trySubA->GetName(), "SubsystemA");

    SubsystemB* trySubB = registry.TryGet<SubsystemB>();
    EXPECT_EQ(trySubB, nullptr);

    registry.ShutdownAll();
}
