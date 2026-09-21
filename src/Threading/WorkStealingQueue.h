#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>

#include "Core/CoreDefines.h"

namespace Engine {
    template<typename T, size_t Capacity = 1024>
    class WorkStealingQueue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

    public:
        WorkStealingQueue() {
            buffer.resize(Capacity);
            top.store(0, std::memory_order_relaxed);
            bottom.store(0, std::memory_order_relaxed);
        }

        bool Push(T item) {
            std::lock_guard<std::mutex> lock(pushPopMutex);
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_acquire);

            if (b - t >= static_cast<int64_t>(Capacity)) {
                return false;
            }

            buffer[b & (Capacity - 1)] = std::move(item);
            std::atomic_thread_fence(std::memory_order_release);
            bottom.store(b + 1, std::memory_order_release);
            return true;
        }

        std::optional<T> Pop() {
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_relaxed);
            if (b <= t) {
                return std::nullopt;
            }

            std::lock_guard<std::mutex> lock(pushPopMutex);
            b = bottom.load(std::memory_order_relaxed) - 1;
            bottom.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            t = top.load(std::memory_order_relaxed);

            if (t <= b) {
                if (t != b) {
                    return std::move(buffer[b & (Capacity - 1)]);
                }

                int64_t expectedTop = t;
                if (!top.compare_exchange_strong(expectedTop, t + 1,
                                                 std::memory_order_seq_cst,
                                                 std::memory_order_relaxed)) {
                    bottom.store(t + 1, std::memory_order_relaxed);
                    return std::nullopt;
                }
                bottom.store(t + 1, std::memory_order_relaxed);
                return std::move(buffer[b & (Capacity - 1)]);
            } else {
                bottom.store(t, std::memory_order_relaxed);
                return std::nullopt;
            }
        }

        std::optional<T> Steal() {
            int64_t t = top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = bottom.load(std::memory_order_acquire);

            if (t < b) {
                int64_t expectedTop = t;
                if (top.compare_exchange_strong(expectedTop, t + 1,
                                                std::memory_order_seq_cst,
                                                std::memory_order_relaxed)) {
                    return std::move(buffer[t & (Capacity - 1)]);
                }
            }
            return std::nullopt;
        }

        ENGINE_NODISCARD size_t Size() const noexcept {
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_relaxed);
            return b >= t ? static_cast<size_t>(b - t) : 0;
        }

        ENGINE_NODISCARD bool Empty() const noexcept { return Size() == 0; }

    private:
        alignas(64) std::atomic<int64_t> top{ 0 };
        alignas(64) std::atomic<int64_t> bottom{ 0 };
        std::vector<T> buffer;
        std::mutex pushPopMutex;
    };
}