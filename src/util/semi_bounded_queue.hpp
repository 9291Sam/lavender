#pragma once

#include "util/log.hpp"
#include "util/threads.hpp"
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <vector>

namespace util
{
    template<class T>
        requires std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>
    class SemiBoundedQueue
    {
    public:
        explicit SemiBoundedQueue(std::size_t lockFreeBufferSize)
            : next_id {0}
            , lock_free_elements_allocated {lockFreeBufferSize}
            , lock_free_buffer {static_cast<std::byte*>(operator new (
                  lockFreeBufferSize * sizeof(T), std::align_val_t {alignof(T)}))}
            , overflow_buffer {{}}
        {}
        ~SemiBoundedQueue() noexcept
        {
            // Also has an implicit unique lock

            std::size_t numberOfElementsDequeued = 0;

            this->dequeueAllRacy(
                [&](const auto&)
                {
                    numberOfElementsDequeued += 1;
                });

            util::assertWarn(
                numberOfElementsDequeued == 0,
                "{} elements still inside of queue that were not dequeued before calling "
                "~SemiBoundedQueue()",
                numberOfElementsDequeued);

            operator delete (
                this->lock_free_buffer,
                sizeof(T) * this->lock_free_elements_allocated,
                std::align_val_t {alignof(T)});
        }

        SemiBoundedQueue(const SemiBoundedQueue&)             = delete;
        SemiBoundedQueue(SemiBoundedQueue&&)                  = delete;
        SemiBoundedQueue& operator= (const SemiBoundedQueue&) = delete;
        SemiBoundedQueue& operator= (SemiBoundedQueue&&)      = delete;

        void enqueue(T t) const noexcept
        {
            const std::size_t maybeThisLockFreeId =
                this->next_id.fetch_add(1, std::memory_order_relaxed);

            if (maybeThisLockFreeId < this->lock_free_elements_allocated)
            {
                new (reinterpret_cast<T*>(this->lock_free_buffer)[maybeThisLockFreeId]) // NOLINT
                    T {std::move(t)};
            }
            else
            {
                // Shit! we can't use the lock free buffer
                this->overflow_buffer.lock(
                    [&](std::vector<T>& overflow)
                    {
                        overflow.push_back(std::move(t));
                    });
            }
        }

        void dequeueAllRacy(const auto& func) noexcept
            requires std::is_nothrow_invocable_v<decltype(func), T&&>
        {
            // this function implicitly requires a unique lock on everything
            const std::size_t numberOfValidElements =
                this->next_id.exchange(0, std::memory_order_acq_rel);

            T* tBuffer = reinterpret_cast<T*>(this->lock_free_buffer); // NOLINT

            for (std::size_t i = 0; i < numberOfValidElements; ++i)
            {
                T* const thisT = tBuffer[i]; // NOLINT
                func(static_cast<T&&>(*thisT));
                thisT->~T();
            }

            this->overflow_buffer.lock(
                [&](std::vector<T>& overflow)
                {
                    if (!overflow.empty())
                    {
                        for (T& t : overflow)
                        {
                            func(static_cast<T&&>(t));
                        }

                        overflow.clear();
                        overflow.shrink_to_fit();
                    }
                });
        }

        mutable std::atomic<std::size_t> next_id;
        std::size_t                      lock_free_elements_allocated;
        std::byte*                       lock_free_buffer;

        util::Mutex<std::vector<T>> overflow_buffer;
    };

    struct LCG
    {
        uint64_t state = 12345;
        uint64_t a     = 1664525;
        uint64_t c     = 1013904223;
        uint64_t m     = 4294967296;

        uint64_t advance()
        {
            state = (a * state + c) % m;
            return state;
        }
    };

    struct XorShift
    {
        uint32_t state;

        uint32_t advance()
        {
            this->state ^= this->state << 13U;
            this->state ^= this->state >> 17U;
            this->state ^= this->state << 5U;

            return this->state;
        }
    };
} // namespace util
