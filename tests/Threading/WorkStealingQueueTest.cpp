#include <gtest/gtest.h>
#include "Threading/WorkStealingQueue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>

using namespace Engine;

TEST(WorkStealingQueueTest, SingleThreadPushPop) {
    WorkStealingQueue<int, 1024> queue;

    // Push 1000 items
    for (int i = 0; i < 1000; ++i) {
        bool pushed = queue.Push(i);
        ASSERT_TRUE(pushed) << "Failed to push item " << i;
    }

    EXPECT_EQ(queue.Size(), 1000u);
    EXPECT_FALSE(queue.Empty());

    // Pop all items in LIFO order
    for (int expected = 999; expected >= 0; --expected) {
        auto item = queue.Pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, expected);
    }

    EXPECT_TRUE(queue.Empty());
    EXPECT_FALSE(queue.Pop().has_value());
}

TEST(WorkStealingQueueTest, ConcurrentStealStress) {
    constexpr size_t kQueueCapacity = 4096;
    constexpr int kBatchCount = 10;
    constexpr int kBatchSize = 1000;
    constexpr int kTotalItems = kBatchCount * kBatchSize; // 10,000 items total

    WorkStealingQueue<int, kQueueCapacity> queue;
    std::vector<std::atomic<int>> processed(kTotalItems);
    for (int i = 0; i < kTotalItems; ++i) {
        processed[i].store(0);
    }

    std::atomic<bool> producerDone{ false };
    std::atomic<int> totalStolen{ 0 };

    constexpr int kStealerCount = 3;
    std::vector<std::thread> stealers;
    stealers.reserve(kStealerCount);

    for (int t = 0; t < kStealerCount; ++t) {
        stealers.emplace_back([&]() {
            while (!producerDone.load(std::memory_order_relaxed) || !queue.Empty()) {
                if (auto item = queue.Steal()) {
                    processed[*item].fetch_add(1, std::memory_order_relaxed);
                    totalStolen.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Owner pushes batches and helps pop
    std::atomic<int> totalPopped{ 0 };
    int currentId = 0;
    for (int b = 0; b < kBatchCount; ++b) {
        for (int i = 0; i < kBatchSize; ++i) {
            while (!queue.Push(currentId)) {
                // If queue full, owner pops local work
                if (auto item = queue.Pop()) {
                    processed[*item].fetch_add(1, std::memory_order_relaxed);
                    totalPopped.fetch_add(1, std::memory_order_relaxed);
                }
            }
            ++currentId;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    producerDone.store(true, std::memory_order_release);

    // Drain remaining by owner
    while (auto item = queue.Pop()) {
        processed[*item].fetch_add(1, std::memory_order_relaxed);
        totalPopped.fetch_add(1, std::memory_order_relaxed);
    }

    for (auto& s : stealers) {
        s.join();
    }

    EXPECT_EQ(totalPopped.load() + totalStolen.load(), kTotalItems);

    for (int i = 0; i < kTotalItems; ++i) {
        EXPECT_EQ(processed[i].load(), 1) << "Item " << i << " was processed " << processed[i].load() << " times";
    }
}
