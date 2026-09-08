#pragma once
#include "Core/ISubsystem.h"
#include "System/Input/InputCodes.h"
#include "System/Input/ActionMapping.h"
#include "System/Events/KeyEvents.h"
#include "System/Events/MouseEvents.h"
#include <bitset>
#include <glm/vec2.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>

namespace Engine {
    class InputSubsystem : public ISubsystem {
    public:
        InputSubsystem() = default;

        std::string_view GetName() const override { return "InputSubsystem"; }
        bool Initialize(SubsystemRegistry& registry) override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        void PushContext(std::string_view name, bool blockLower = true);
        void PopContext();
        std::string_view GetCurrentContextName() const noexcept;

        bool IsKeyPressed(KeyCode key) const noexcept { return currentKeys.test(static_cast<size_t>(key)); }
        bool IsKeyJustPressed(KeyCode key) const noexcept {
            return currentKeys.test(static_cast<size_t>(key)) && !previousKeys.test(static_cast<size_t>(key));
        }
        bool IsKeyJustReleased(KeyCode key) const noexcept {
            return !currentKeys.test(static_cast<size_t>(key)) && previousKeys.test(static_cast<size_t>(key));
        }

        bool IsMouseButtonPressed(MouseButton button) const noexcept { return currentMouseButtons.test(static_cast<size_t>(button)); }
        bool IsMouseButtonJustPressed(MouseButton button) const noexcept {
            return currentMouseButtons.test(static_cast<size_t>(button)) && !previousMouseButtons.test(static_cast<size_t>(button));
        }
        bool IsMouseButtonJustReleased(MouseButton button) const noexcept {
            return !currentMouseButtons.test(static_cast<size_t>(button)) && previousMouseButtons.test(static_cast<size_t>(button));
        }

        glm::vec2 GetMousePosition() const noexcept { return mousePosition; }
        glm::vec2 GetMouseDelta() const noexcept { return mouseDelta; }
        float GetMouseScrollDelta() const noexcept { return mouseScroll; }

        void BindAction(std::string_view actionName, const ActionBinding& binding);
        void BindAxis(std::string_view axisName, const AxisBinding& binding);

        bool IsActionTriggered(std::string_view actionName) const;
        float GetAxis(std::string_view axisName) const;
        glm::vec2 GetVector2D(std::string_view axisX, std::string_view axisY) const;

    private:
        void OnKeyPressed(KeyPressedEvent& e);
        void OnKeyReleased(KeyReleasedEvent& e);
        void OnMouseButtonPressed(MouseButtonPressedEvent& e);
        void OnMouseButtonReleased(MouseButtonReleasedEvent& e);
        void OnMouseMoved(MouseMovedEvent& e);
        void OnMouseScrolled(MouseScrolledEvent& e);
        void OnMouseRawDelta(MouseRawDeltaEvent& e);

        std::bitset<512> currentKeys;
        std::bitset<512> previousKeys;
        std::bitset<16> currentMouseButtons;
        std::bitset<16> previousMouseButtons;

        glm::vec2 mousePosition{ 0.0f, 0.0f };
        glm::vec2 mouseDelta{ 0.0f, 0.0f };
        float mouseScroll = 0.0f;

        std::vector<InputContext> contextStack;
        TransparentStringMap<ActionBinding> globalActionBindings;
        TransparentStringMap<AxisBinding> globalAxisBindings;

        std::vector<SubscriptionToken> listenerTokens;
    };
}