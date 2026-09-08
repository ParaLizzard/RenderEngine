#include <gtest/gtest.h>
#include "System/Window/WindowSubsystem.h"
#include "System/Events/EventDispatcher.h"
#include "System/Events/WindowEvents.h"
#include "Core/SubsystemRegistry.h"

TEST(WindowSubsystemTest, WindowSubsystemLifecycle) {
    Engine::SubsystemRegistry registry;
    Engine::WindowProps props{
        .title = "TestWindow",
        .width = 1280,
        .height = 720,
        .cursorMode = Engine::CursorMode::Normal
    };

    auto& windowSub = registry.Register<Engine::WindowSubsystem>(props);
    EXPECT_EQ(windowSub.GetName(), "WindowSubsystem");

    EXPECT_TRUE(registry.InitializeAll());

    Engine::IWindow& window = windowSub.GetWindow();
    EXPECT_EQ(window.GetWidth(), 1280u);
    EXPECT_EQ(window.GetHeight(), 720u);

    registry.ShutdownAll();
}

TEST(WindowSubsystemTest, WindowEventEmissionAndPayload) {
    auto& bus = Engine::EventDispatcher::Get();

    // 1. Resize event
    bool resizeHandled = false;
    auto tokenResize = bus.Subscribe<Engine::WindowResizeEvent>([&](Engine::WindowResizeEvent& e) {
        resizeHandled = true;
        EXPECT_EQ(e.GetWidth(), 2560u);
        EXPECT_EQ(e.GetHeight(), 1440u);
    });

    Engine::WindowResizeEvent resizeEvent(2560, 1440);
    bus.PostEvent(resizeEvent);
    EXPECT_TRUE(resizeHandled);
    bus.Unsubscribe(tokenResize);

    // 2. DPI Content Scale event
    bool dpiHandled = false;
    auto tokenDPI = bus.Subscribe<Engine::WindowContentScaleEvent>([&](Engine::WindowContentScaleEvent& e) {
        dpiHandled = true;
        EXPECT_FLOAT_EQ(e.GetScaleX(), 1.5f);
        EXPECT_FLOAT_EQ(e.GetScaleY(), 1.5f);
    });

    Engine::WindowContentScaleEvent dpiEvent(1.5f, 1.5f);
    bus.PostEvent(dpiEvent);
    EXPECT_TRUE(dpiHandled);
    bus.Unsubscribe(tokenDPI);

    // 3. File drop event
    bool dropHandled = false;
    auto tokenDrop = bus.Subscribe<Engine::WindowDropFilesEvent>([&](Engine::WindowDropFilesEvent& e) {
        dropHandled = true;
        ASSERT_EQ(e.GetFiles().size(), 2u);
        EXPECT_EQ(e.GetFiles()[0], "models/sponza.gltf");
        EXPECT_EQ(e.GetFiles()[1], "textures/diffuse.png");
    });

    Engine::WindowDropFilesEvent dropEvent(std::vector<std::string>{"models/sponza.gltf", "textures/diffuse.png"});
    bus.PostEvent(dropEvent);
    EXPECT_TRUE(dropHandled);
    bus.Unsubscribe(tokenDrop);
}
