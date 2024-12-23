#include "timer.hpp"
#include "log.hpp"
#include <chrono>
#include <source_location>

namespace util
{
    Timer::Timer(std::string name_, std::source_location loc)
        : name {std::move(name_)}
        , start {std::chrono::steady_clock::now()}
        , location {loc}
        , ended {false}
    {}

    Timer::~Timer()
    {
        this->end();
    }

    std::size_t Timer::end(bool shouldPrint)
    {
        if (!this->ended)
        {
            this->ended = true;

            std::chrono::microseconds durationUs =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - this->start);

            if (shouldPrint)
            {
                util::logTrace<const char*, const std::chrono::microseconds&>(
                    "{} | {}", this->name.data(), durationUs, this->location);
            }

            return static_cast<std::size_t>(durationUs.count());
        }

        return 0;
    }

    MultiTimer::MultiTimer(std::source_location location_)
        : previous_stamp_time {std::chrono::steady_clock::now()}
        , location {location_}
    {}

    MultiTimer::~MultiTimer()
    {
        const std::string maybeMessage = this->finish();

        if (!maybeMessage.empty())
        {
            util::logTrace("{}", maybeMessage);
        }
    }

    void MultiTimer::stamp(std::string name)
    {
        const std::chrono::time_point<std::chrono::steady_clock> now =
            std::chrono::steady_clock::now();

        const std::chrono::microseconds thisDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(now - this->previous_stamp_time);

        this->timings.push_back(Timing {.name {std::move(name)}, .time {thisDuration}});

        this->previous_stamp_time = now;
    }

    std::string MultiTimer::finish()
    {
        std::string output {};

        for (const Timing& t : this->timings)
        {
            output += static_cast<std::string>(t);

            output += " | ";
        }

        if (!output.empty())
        {
            output.pop_back();
            output.pop_back();
            output.pop_back();
        }

        this->timings.clear();

        return output;
    }

    MultiTimer::Timing::operator std::string () const
    {
        return std::format("{} {}us", this->name, this->time.count());
    }
} // namespace util
