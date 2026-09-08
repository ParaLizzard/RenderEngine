#include <gtest/gtest.h>
#include "System/Input/InputSubsystem.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/KeyEvents.h"
#include "System/Events/MouseEvents.h"
#include "Core/SubsystemRegistry.h"
#include <glm/geometric.hpp>

class InputSubsystemTest : public ::testing::Test {
protected:
    Engine::SubsystemRegistry registry;
    Engine::InputSubsystem* input = nullptr;

    void SetUp() override {
        input = &registry.Register<Engine::InputSubsystem>();
        ASSERT_TRUE(registry.InitializeAll());
    }

    void TearDown() override {
        registry.ShutdownAll();
    }
};

TEST_F(InputSubsystemTest, BitsetStatePolling) {
    auto& bus = Engine::EventDispatcher::Get();

    // Frame 0: Press Space
    Engine::KeyPressedEvent pressSpace(Engine::KeyCode::Space, false);
    bus.PostEvent(pressSpace);

    EXPECT_TRUE(input->IsKeyPressed(Engine::KeyCode::Space));
    EXPECT_TRUE(input->IsKeyJustPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustReleased(Engine::KeyCode::Space));

    // End of Frame 0 -> Transition to Frame 1
    input->Update(0.016f);

    // Frame 1: Space is still held
    EXPECT_TRUE(input->IsKeyPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustReleased(Engine::KeyCode::Space));

    // Frame 2: Release Space
    Engine::KeyReleasedEvent releaseSpace(Engine::KeyCode::Space);
    bus.PostEvent(releaseSpace);

    EXPECT_FALSE(input->IsKeyPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustPressed(Engine::KeyCode::Space));
    EXPECT_TRUE(input->IsKeyJustReleased(Engine::KeyCode::Space));

    // End of Frame 2 -> Transition to Frame 3
    input->Update(0.016f);

    EXPECT_FALSE(input->IsKeyPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustPressed(Engine::KeyCode::Space));
    EXPECT_FALSE(input->IsKeyJustReleased(Engine::KeyCode::Space));
}

TEST_F(InputSubsystemTest, ActionTriggerTypes) {
    auto& bus = Engine::EventDispatcher::Get();

    // 1. Continuous 'Pressed' action
    input->BindAction("Sprint", Engine::ActionBinding{
        .keys = { Engine::KeyCode::LeftShift },
        .triggerType = Engine::TriggerType::Pressed
    });

    // 2. Pulse 'JustPressed' action
    input->BindAction("Jump", Engine::ActionBinding{
        .keys = { Engine::KeyCode::Space },
        .triggerType = Engine::TriggerType::JustPressed
    });

    // Frame 0: Press both keys
    Engine::KeyPressedEvent pressShift(Engine::KeyCode::LeftShift, false);
    Engine::KeyPressedEvent pressSpace(Engine::KeyCode::Space, false);
    bus.PostEvent(pressShift);
    bus.PostEvent(pressSpace);

    EXPECT_TRUE(input->IsActionTriggered("Sprint"));
    EXPECT_TRUE(input->IsActionTriggered("Jump"));

    // Frame 1: Update
    input->Update(0.016f);

    // Sprint (continuous) remains true; Jump (pulse) becomes false!
    EXPECT_TRUE(input->IsActionTriggered("Sprint"));
    EXPECT_FALSE(input->IsActionTriggered("Jump"));
}

TEST_F(InputSubsystemTest, ChordModifierKeyValidation) {
    auto& bus = Engine::EventDispatcher::Get();

    // Bind "Save" to Ctrl + S
    input->BindAction("Save", Engine::ActionBinding{
        .keys = { Engine::KeyCode::S },
        .modifierKey = Engine::KeyCode::LeftControl,
        .triggerType = Engine::TriggerType::JustPressed
    });

    // Press S alone without Ctrl -> Should NOT trigger
    Engine::KeyPressedEvent pressS(Engine::KeyCode::S, false);
    bus.PostEvent(pressS);
    EXPECT_FALSE(input->IsActionTriggered("Save"));

    // Now press LeftControl
    Engine::KeyPressedEvent pressCtrl(Engine::KeyCode::LeftControl, false);
    bus.PostEvent(pressCtrl);
    EXPECT_TRUE(input->IsActionTriggered("Save"));
}

TEST_F(InputSubsystemTest, Vector2DCircularNormalization) {
    auto& bus = Engine::EventDispatcher::Get();

    input->BindAxis("MoveForward", Engine::AxisBinding{
        .keyScales = { { Engine::KeyCode::W, 1.0f } }
    });
    input->BindAxis("MoveRight", Engine::AxisBinding{
        .keyScales = { { Engine::KeyCode::D, 1.0f } }
    });

    // Press both W and D (diagonal motion)
    Engine::KeyPressedEvent pressW(Engine::KeyCode::W, false);
    Engine::KeyPressedEvent pressD(Engine::KeyCode::D, false);
    bus.PostEvent(pressW);
    bus.PostEvent(pressD);

    glm::vec2 move2D = input->GetVector2D("MoveRight", "MoveForward");

    // Length must be strictly clamped to <= 1.0
    float length = glm::length(move2D);
    EXPECT_NEAR(length, 1.0f, 1e-4f);
    EXPECT_NEAR(move2D.x, 0.707106f, 1e-3f);
    EXPECT_NEAR(move2D.y, 0.707106f, 1e-3f);
}

TEST_F(InputSubsystemTest, ContextStackMasking) {
    auto& bus = Engine::EventDispatcher::Get();

    input->BindAction("Fire", Engine::ActionBinding{
        .mouseButtons = { Engine::MouseButton::Left }
    });

    // Click left mouse in default gameplay context
    Engine::MouseButtonPressedEvent click(Engine::MouseButton::Left);
    bus.PostEvent(click);
    EXPECT_TRUE(input->IsActionTriggered("Fire"));

    // Push UI context that blocks lower gameplay layers
    input->PushContext("ModalMenu", /*blockLower=*/true);

    EXPECT_FALSE(input->IsActionTriggered("Fire"));

    // Pop modal context
    input->PopContext();

    EXPECT_TRUE(input->IsActionTriggered("Fire"));
}

TEST_F(InputSubsystemTest, RawMouseDeltaAccumulation) {
    auto& bus = Engine::EventDispatcher::Get();

    // Two high-polling micro-movements within one frame
    Engine::MouseRawDeltaEvent delta1(10.0f, 5.0f);
    Engine::MouseRawDeltaEvent delta2(5.0f, -2.0f);
    bus.PostEvent(delta1);
    bus.PostEvent(delta2);

    glm::vec2 delta = input->GetMouseDelta();
    EXPECT_FLOAT_EQ(delta.x, 15.0f);
    EXPECT_FLOAT_EQ(delta.y, 3.0f);

    // End of frame resets delta
    input->Update(0.016f);

    glm::vec2 resetDelta = input->GetMouseDelta();
    EXPECT_FLOAT_EQ(resetDelta.x, 0.0f);
    EXPECT_FLOAT_EQ(resetDelta.y, 0.0f);
}

TEST_F(InputSubsystemTest, ActionAxisDesignatedBindings) {
    auto& bus = Engine::EventDispatcher::Get();

    // Bind using designated initializers
    input->BindAxis("MoveForward", Engine::AxisBinding{
        .positiveKey = Engine::KeyCode::W,
        .negativeKey = Engine::KeyCode::S,
        .scale = 1.0f
    });

    input->BindAction("ToggleSSAO", Engine::ActionBinding{
        .primaryKey = Engine::KeyCode::O,
        .triggerType = Engine::TriggerType::JustPressed
    });

    // Press W -> Axis should be +1.0
    Engine::KeyPressedEvent pressW(Engine::KeyCode::W, false);
    bus.PostEvent(pressW);
    EXPECT_FLOAT_EQ(input->GetAxis("MoveForward"), 1.0f);

    // Press S while W is held -> Axis should be 0.0
    Engine::KeyPressedEvent pressS(Engine::KeyCode::S, false);
    bus.PostEvent(pressS);
    EXPECT_FLOAT_EQ(input->GetAxis("MoveForward"), 0.0f);

    // Release W -> Axis should be -1.0
    Engine::KeyReleasedEvent releaseW(Engine::KeyCode::W);
    bus.PostEvent(releaseW);
    EXPECT_FLOAT_EQ(input->GetAxis("MoveForward"), -1.0f);

    // Action toggle check with primaryKey
    EXPECT_FALSE(input->IsActionTriggered("ToggleSSAO"));
    Engine::KeyPressedEvent pressO(Engine::KeyCode::O, false);
    bus.PostEvent(pressO);
    EXPECT_TRUE(input->IsActionTriggered("ToggleSSAO"));

    // Next frame -> JustPressed should become false
    input->Update(0.016f);
    EXPECT_FALSE(input->IsActionTriggered("ToggleSSAO"));
}

