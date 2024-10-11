#pragma once

#include <atomic>
#include <type_traits>

namespace util
{
    template<class T>
        requires std::is_trivially_copyable_v<T>
    T atomicAbaAdd(std::atomic<T>& f, T d)
    {
        T old     = f.load(std::memory_order_consume);
        T desired = old + d;
        while (!f.compare_exchange_weak(
            old, desired, std::memory_order_release, std::memory_order_consume))
        {
            desired = old + d;
        }
        return desired;
    }
} // namespace util
