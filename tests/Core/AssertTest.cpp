#include <gtest/gtest.h>
#include "Core/Assert.h"

TEST(AssertTest, VerifyEvaluatesConditionInRelease) {
    bool executed = false;
    ENGINE_VERIFY((executed = true) == true, "Verify expression must always execute");
    EXPECT_TRUE(executed);
}

TEST(AssertTest, VerifyPassingConditionDoesNotBreak) {
    int value = 42;
    ENGINE_VERIFY(value == 42, "Value should be 42, got {}", value);
    EXPECT_EQ(value, 42);
}

TEST(AssertTest, AssertPassingConditionExecutes) {
    int value = 100;
    ENGINE_ASSERT(value == 100, "Assert condition must hold");
    EXPECT_EQ(value, 100);
}

TEST(AssertTest, VerifyFailingConditionTriggersDeath) {
    EXPECT_DEATH(
        ENGINE_VERIFY(false, "Expected test verification failure"),
        "Expected test verification failure"
    );
}
