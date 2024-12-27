#include "thread_pool.hpp"
#include <atomic>
#include <functional>

namespace util
{

    ThreadPool::ThreadPool(std::size_t numberOfWorkers)
    {
        this->threads.reserve(numberOfWorkers);

        for (std::size_t i = 0; i < numberOfWorkers; ++i)
        {
            this->threads.push_back(std::thread {&ThreadPool::threadFunction, this});
        }
    }

    ThreadPool::~ThreadPool()
    {
        this->should_threads_close.store(true, std::memory_order_release);

        for (std::thread& t : this->threads)
        {
            t.join();
        }

        std::function<void()> localFunction {};

        for (int i = 0; i < 64; ++i)
        {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            while (this->function_queue.try_dequeue(localFunction))
            {
                localFunction();

                localFunction = nullptr;
            }
        }
    }

    void ThreadPool::threadFunction() const
    {
        using namespace std::literals;

        std::function<void()> localFunction {};

        while (!this->should_threads_close.load(std::memory_order_acquire))
        {
            this->function_queue.wait_dequeue_timed(localFunction, 10ms);

            if (localFunction != nullptr)
            {
                localFunction();

                localFunction = nullptr;
            }
        }
    }

    namespace
    {
        std::atomic<ThreadPool*> GlobalThreadPool = nullptr; // NOLINT
    } // namespace

    void installGlobalThreadPoolRacy()
    {
        GlobalThreadPool.store(new ThreadPool {});           // NOLINT
        std::atomic_thread_fence(std::memory_order_release); // this lets the load be relaxed
    }

    void removeGlobalThreadPoolRacy()
    {
        delete GlobalThreadPool; // NOLINT
    }

    const ThreadPool* getGlobalThreadPool() // NOLINT
    {
        return GlobalThreadPool.load(std::memory_order_relaxed);
    }
} // namespace util