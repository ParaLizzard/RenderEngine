#pragma once
#include "System/Events/Event.h"
#include "Core/Assert.h"
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <utility>

namespace Engine {

    class EventDispatcher;

    class ScopedSubscription {
    public:
        ScopedSubscription() = default;
        ScopedSubscription(EventDispatcher& dispatcher, SubscriptionToken token)
            : dispatcher(&dispatcher), token(token) {}

        ~ScopedSubscription();
        
        ENGINE_NON_COPYABLE(ScopedSubscription);

        ScopedSubscription(ScopedSubscription&& other) noexcept
            : dispatcher(other.dispatcher), token(other.token) {
            other.dispatcher = nullptr;
            other.token = 0;
        }

        // Resets if moved
        ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
            if (this != &other) {
                Reset();
                dispatcher = other.dispatcher;
                token = other.token;
                other.dispatcher = nullptr;
                other.token = 0;
            }
            return *this;
        }

        // Unsubscribes from its token and dispatcher
        void Reset();

        // Get subscription token
        ENGINE_NODISCARD SubscriptionToken GetToken() const noexcept { return token; }

        // Scoped subscription is invalid if it doesn't have its token and dispatcher
        ENGINE_NODISCARD bool IsValid() const noexcept { return token != 0 && dispatcher != nullptr; }

    private:
        EventDispatcher* dispatcher = nullptr;
        SubscriptionToken token = 0;
    };

    class EventDispatcher {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        // Get instance
        static EventDispatcher& Get();

        // Subscribes the callback to event dispatcher
        template<typename T, typename F>
        SubscriptionToken Subscribe(F&& callback) {
            static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");
            EventTypeID typeId = T::GetStaticType();
            SubscriptionToken token = nextToken.fetch_add(1, std::memory_order_relaxed);

            std::unique_lock<std::shared_mutex> lock(listenersMutex);
            listeners[typeId].push_back({
                token,
                [cb = std::forward<F>(callback)](Event& e) {
                    if (!e.handled) {
                        cb(static_cast<T&>(e));
                    }
                }
            });
            tokenToType[token] = typeId;
            return token;
        }

        template<typename T, typename F>
        ScopedSubscription SubscribeScoped(F&& callback) {
            SubscriptionToken token = Subscribe<T>(std::forward<F>(callback));
            return {*this, token};
        }

        // Unsubscribes the callback from the event dispatcher
        void Unsubscribe(SubscriptionToken token) {
            if (token == 0) return;

            std::unique_lock<std::shared_mutex> lock(listenersMutex);
            auto itType = tokenToType.find(token);
            if (itType != tokenToType.end()) {
                EventTypeID typeId = itType->second;
                auto itList = listeners.find(typeId);
                if (itList != listeners.end()) {
                    std::erase_if(itList->second, [token](const auto& entry) {
                        return entry.first == token;
                    });
                    if (itList->second.empty()) {
                        listeners.erase(itList);
                    }
                }
                tokenToType.erase(itType);
            }
        }

        // Synchronous event dispatch
        void PostEvent(Event& event) {
            EventTypeID typeId = event.GetEventType();

            std::vector<EventCallbackFn> callbacksToInvoke;

            {
                std::shared_lock<std::shared_mutex> lock(listenersMutex);
                auto it = listeners.find(typeId);
                if (it != listeners.end()) {
                    callbacksToInvoke.reserve(it->second.size());
                    for (const auto& [token, cb] : it->second) {
                        callbacksToInvoke.push_back(cb);
                    }
                }
            }

            for (const auto& callback : callbacksToInvoke) {
                callback(event);
                if (event.handled) break;
            }
        }

        // Thread-safe event dispatch
        void PostEventThreadSafe(std::unique_ptr<Event> event) {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push_back(std::move(event));
        }

        // Drains all thread-safe queued events
        void DispatchQueuedEvents() {
            std::vector<std::unique_ptr<Event>> localQueue;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                localQueue.swap(eventQueue);
            }
            for (auto& event : localQueue) {
                if (event) PostEvent(*event);
            }
        }

    private:
        EventDispatcher() = default;

        std::unordered_map<EventTypeID, std::vector<std::pair<SubscriptionToken, EventCallbackFn>>> listeners;
        std::unordered_map<SubscriptionToken, EventTypeID> tokenToType;
        mutable std::shared_mutex listenersMutex;

        std::vector<std::unique_ptr<Event>> eventQueue;
        std::mutex queueMutex;
        std::atomic<SubscriptionToken> nextToken{1};
    };

    inline ScopedSubscription::~ScopedSubscription() {
        Reset();
    }

    inline void ScopedSubscription::Reset() {
        if (dispatcher && token != 0) {
            dispatcher->Unsubscribe(token);
            dispatcher = nullptr;
            token = 0;
        }
    }

} // namespace Engine