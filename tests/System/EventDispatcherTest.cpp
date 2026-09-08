#include <gtest/gtest.h>
#include "System/Events/EventDispatcher.h"
#include "System/Events/WindowEvents.h"
#include "System/Events/KeyEvents.h"
#include "System/Events/MouseEvents.h"
#include <thread>
#include <vector>
#include <unordered_set>
#include <atomic>

class EventDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any residual queued events
        Engine::EventDispatcher::Get().DispatchQueuedEvents();
    }
};

TEST_F(EventDispatcherTest, ImmediateSyncDispatch) {
    auto& bus = Engine::EventDispatcher::Get();
    bool received = false;
    uint32_t width = 0;
    uint32_t height = 0;

    auto token = bus.Subscribe<Engine::WindowResizeEvent>([&](Engine::WindowResizeEvent& e) {
        received = true;
        width = e.GetWidth();
        height = e.GetHeight();
    });

    Engine::WindowResizeEvent event(1920, 1080);
    bus.PostEvent(event);

    EXPECT_TRUE(received);
    EXPECT_EQ(width, 1920u);
    EXPECT_EQ(height, 1080u);

    bus.Unsubscribe(token);
}

TEST_F(EventDispatcherTest, HandledEventPropagationHalt) {
    auto& bus = Engine::EventDispatcher::Get();
    bool firstHandlerCalled = false;
    bool secondHandlerCalled = false;

    auto token1 = bus.Subscribe<Engine::KeyPressedEvent>([&](Engine::KeyPressedEvent& e) {
        firstHandlerCalled = true;
        e.handled = true; // Consume event
    });

    auto token2 = bus.Subscribe<Engine::KeyPressedEvent>([&](Engine::KeyPressedEvent&) {
        secondHandlerCalled = true;
    });

    Engine::KeyPressedEvent event(Engine::KeyCode::Space, false);
    bus.PostEvent(event);

    EXPECT_TRUE(firstHandlerCalled);
    EXPECT_FALSE(secondHandlerCalled);

    bus.Unsubscribe(token1);
    bus.Unsubscribe(token2);
}

TEST_F(EventDispatcherTest, ScopedSubscriptionAutoUnsubscribe) {
    auto& bus = Engine::EventDispatcher::Get();
    int callCount = 0;

    {
        Engine::ScopedSubscription scoped = bus.SubscribeScoped<Engine::KeyReleasedEvent>([&](Engine::KeyReleasedEvent&) {
            callCount++;
        });

        EXPECT_TRUE(scoped.IsValid());

        Engine::KeyReleasedEvent event1(Engine::KeyCode::A);
        bus.PostEvent(event1);
        EXPECT_EQ(callCount, 1);
    } // scoped destroyed here -> automatically unsubscribes

    Engine::KeyReleasedEvent event2(Engine::KeyCode::A);
    bus.PostEvent(event2);
    EXPECT_EQ(callCount, 1); // Should not increase
}

TEST_F(EventDispatcherTest, ConcurrentAsyncQueueDrain) {
    auto& bus = Engine::EventDispatcher::Get();
    std::atomic<int> receivedCount{0};

    auto token = bus.Subscribe<Engine::MouseMovedEvent>([&](Engine::MouseMovedEvent&) {
        receivedCount.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int kThreadCount = 4;
    constexpr int kEventsPerThread = 500;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&bus]() {
            for (int i = 0; i < kEventsPerThread; ++i) {
                bus.PostEventThreadSafe(std::make_unique<Engine::MouseMovedEvent>(10.0f, 20.0f));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Now drain queue on main thread
    bus.DispatchQueuedEvents();

    EXPECT_EQ(receivedCount.load(), kThreadCount * kEventsPerThread);

    bus.Unsubscribe(token);
}

TEST_F(EventDispatcherTest, EventTypeIDUniquenessCheck) {
    std::unordered_set<Engine::EventTypeID> typeIDs;

    auto insertCheck = [&](Engine::EventTypeID id) {
        EXPECT_TRUE(typeIDs.insert(id).second) << "Duplicate EventTypeID detected: " << id;
    };

    insertCheck(Engine::WindowResizeEvent::GetStaticType());
    insertCheck(Engine::WindowCloseEvent::GetStaticType());
    insertCheck(Engine::WindowFocusEvent::GetStaticType());
    insertCheck(Engine::WindowIconifyEvent::GetStaticType());
    insertCheck(Engine::WindowDropFilesEvent::GetStaticType());
    insertCheck(Engine::WindowContentScaleEvent::GetStaticType());

    insertCheck(Engine::KeyPressedEvent::GetStaticType());
    insertCheck(Engine::KeyReleasedEvent::GetStaticType());
    insertCheck(Engine::KeyTypedEvent::GetStaticType());

    insertCheck(Engine::MouseMovedEvent::GetStaticType());
    insertCheck(Engine::MouseScrolledEvent::GetStaticType());
    insertCheck(Engine::MouseRawDeltaEvent::GetStaticType());
    insertCheck(Engine::MouseEnterEvent::GetStaticType());
    insertCheck(Engine::MouseButtonPressedEvent::GetStaticType());
    insertCheck(Engine::MouseButtonReleasedEvent::GetStaticType());

    EXPECT_EQ(typeIDs.size(), 15u);
}
