#pragma once

#include <chrono>
#include <source_location>
#include <string>
#include <vector>

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

        /// Returns the number of microseconds this action took
        std::size_t end(bool shouldPrint = true);

    private:
        std::string                                        name;
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::source_location                               location;
        bool                                               ended;
    };

    class MultiTimer
    {
    public:
        explicit MultiTimer(std::source_location loc = std::source_location::current());
        ~MultiTimer();

        MultiTimer(const MultiTimer&)             = delete;
        MultiTimer(MultiTimer&&)                  = delete;
        MultiTimer& operator= (const MultiTimer&) = delete;
        MultiTimer& operator= (MultiTimer&&)      = delete;

        void                      stamp(std::string name);
        [[nodiscard]] std::string finish();

    private:
        struct Timing
        {
            std::string               name;
            std::chrono::microseconds time;

            explicit operator std::string () const;
        };
        std::vector<Timing> timings;

        std::chrono::time_point<std::chrono::steady_clock> previous_stamp_time;
        std::source_location                               location;
    };
} // namespace util
