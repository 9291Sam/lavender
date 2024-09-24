#pragma once

#include "util/log.hpp"
#include <chrono>
#include <string>

namespace util
{
    class Timer
    {
    public:
        explicit Timer(std::string name_)
            : name {std::move(name_)}
            , start {std::chrono::steady_clock::now()}
        {}
        ~Timer()
        {
            util::logTrace(
                "{} | {}",
                this->name,
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - this->start));
        }

        Timer(const Timer&)             = delete;
        Timer(Timer&&)                  = delete;
        Timer& operator= (const Timer&) = delete;
        Timer& operator= (Timer&&)      = delete;

    private:
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::string                                        name;
    };
} // namespace util