#pragma once

#include "util/log.hpp"
#include <chrono>
#include <source_location>
#include <string>

namespace util
{
    class Timer
    {
    public:
        explicit Timer(
            std::string          name_,
            std::source_location loc = std::source_location::current())
            : name {std::move(name_)}
            , start {std::chrono::steady_clock::now()}
            , location {loc}
        {}
        ~Timer()
        {
            util::logTrace<const char*, std::chrono::microseconds>(
                "{} | {}",
                this->name.data(),
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - this->start),
                this->location);
        }

        Timer(const Timer&)             = delete;
        Timer(Timer&&)                  = delete;
        Timer& operator= (const Timer&) = delete;
        Timer& operator= (Timer&&)      = delete;

    private:
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::string                                        name;
        std::source_location                               location;
    };
} // namespace util