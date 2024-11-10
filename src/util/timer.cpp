#include "timer.hpp"
#include "log.hpp"

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

    void Timer::end()
    {
        if (!this->ended)
        {
            this->ended = true;

            util::logTrace<const char*, std::chrono::microseconds>(
                "{} | {}",
                this->name.data(),
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - this->start),
                this->location);
        }
    }
} // namespace util
