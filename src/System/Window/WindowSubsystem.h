#pragma once
#include "Core/ISubsystem.h"
#include "System/Window/IWindow.h"
#include <memory>

namespace Engine {
    class WindowSubsystem : public ISubsystem {
    public:
        explicit WindowSubsystem(const WindowProps& props = {});
        ~WindowSubsystem() override = default;

        ENGINE_NODISCARD std::string_view GetName() const override { return "WindowSubsystem"; }
        bool Initialize(SubsystemRegistry& registry) override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        IWindow& GetWindow() noexcept { return *window; }

    private:
        WindowProps properties;
        std::unique_ptr<IWindow> window;
    };
}