#include "timer.hpp"
#include "log.hpp"
#include <chrono>

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

    std::size_t Timer::end()
    {
        if (!this->ended)
        {
            this->ended = true;

            std::chrono::microseconds durationUs =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - this->start);

            util::logTrace<const char*, const std::chrono::microseconds&>(
                "{} | {}", this->name.data(), durationUs, this->location);

            return static_cast<std::size_t>(durationUs.count());
        }

        return 0;
    }
} // namespace util
