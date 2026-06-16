#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace Engine
{
    class JobSystemStoppedException : public std::runtime_error
    {
    public:
        JobSystemStoppedException() : std::runtime_error("JobSystem has been stopped.") {}
    };

    class JobSystem
    {
    public:
        JobSystem(size_t numThreads);
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop = false;
    };


    inline JobSystem::JobSystem(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);


                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        if (this->stop && this->tasks.empty())
                            return;

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    template<class F, class... Args>
    auto JobSystem::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using return_type = typename std::invoke_result_t<F, Args...>;

        // 1. Bind the function and its arguments together
        auto bound_task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // 2. Wrap the execution with a try-catch block to surface exceptions immediately
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [bound_task = std::move(bound_task)]() mutable {
                try {
                    return bound_task();
                } catch (const std::exception& e) {
                    std::cerr << "\n[JobSystem ERROR]: Exception thrown inside worker thread: "
                              << e.what() << std::endl;
                    throw;
                } catch (...) {
                    std::cerr << "\n[JobSystem ERROR]: Unknown exception thrown inside worker thread."
                              << std::endl;
                    throw;
                }
            }
        );

        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stop)
                throw std::runtime_error("JobSystem: enqueue called on stopped JobSystem");

            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one();
        return res;
    }


    inline JobSystem::~JobSystem()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all();

        for (std::thread& worker : workers)
        {
            worker.join();
        }
    }
}