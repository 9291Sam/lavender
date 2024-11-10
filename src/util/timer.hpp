#pragma once

#include <chrono>
#include <source_location>
#include <string>

namespace util
{
    class Timer
    {
    public:
        explicit Timer(
            std::string name_, std::source_location loc = std::source_location::current());
        ~Timer();

        Timer(const Timer&)             = delete;
        Timer(Timer&&)                  = delete;
        Timer& operator= (const Timer&) = delete;
        Timer& operator= (Timer&&)      = delete;

        void end();

    private:
        std::string                                        name;
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::source_location                               location;
        bool                                               ended;
    };
} // namespace util
