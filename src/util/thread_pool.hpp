#pragma once

#include "timer.hpp"
#include <blockingconcurrentqueue.h>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>

namespace util
{
    class ThreadPool
    {
    public:
        explicit ThreadPool(std::size_t numberOfWorkers = std::thread::hardware_concurrency() - 2);
        ~ThreadPool();

        ThreadPool(const ThreadPool&)             = delete;
        ThreadPool(ThreadPool&&)                  = delete;
        ThreadPool& operator= (const ThreadPool&) = delete;
        ThreadPool& operator= (ThreadPool&&)      = delete;

        template<class Fn, class R = std::invoke_result_t<Fn>>
        std::future<R> executeOnPool(Fn&& func) const
            requires std::is_invocable_v<Fn>
        {
            std::shared_ptr<std::packaged_task<R()>> task {
                new std::packaged_task<R()> {std::forward<Fn>(func)}};
            std::future<R> future = task->get_future();

            this->function_queue.enqueue(
                std::function<void()> {[localTask = std::move(task)]() mutable
                                       {
                                           (*localTask)();
                                       }});

            return future;
        }
    private:
        void threadFunction() const;

        mutable moodycamel::BlockingConcurrentQueue<std::function<void()>> function_queue;
        std::atomic<bool>                                                  should_threads_close;
        std::vector<std::thread>                                           threads;
    };

    void              installGlobalThreadPoolRacy();
    void              removeGlobalThreadPoolRacy();
    const ThreadPool* getGlobalThreadPool();

    template<class Fn>
    std::future<std::invoke_result_t<Fn>> runAsync(Fn func)
        requires std::is_invocable_v<Fn>
    {
        return getGlobalThreadPool()->executeOnPool(std::forward<Fn>(func));
    }
} // namespace util