#pragma once

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
        explicit ThreadPool(std::size_t numberOfWorkers = std::thread::hardware_concurrency());
        ~ThreadPool();

        ThreadPool(const ThreadPool&)             = delete;
        ThreadPool(ThreadPool&&)                  = delete;
        ThreadPool& operator= (const ThreadPool&) = delete;
        ThreadPool& operator= (ThreadPool&&)      = delete;

        template<class Fn>
        std::future<std::invoke_result_t<Fn>> executeOnPool(Fn func) const
            requires std::is_invocable_v<Fn>
        {
            std::packaged_task                    task {func};
            std::future<std::invoke_result_t<Fn>> future = task.get_future();

            this->function_queue.enqueue(
                std::move_only_function {[localTask = std::move(task)] mutable
                                         {
                                             localTask();
                                         }});

            return future;
        }
    private:
        void threadFunction() const;

        mutable moodycamel::BlockingConcurrentQueue<std::move_only_function<void()>> function_queue;
        std::atomic<bool>        should_threads_close;
        std::vector<std::thread> threads;
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