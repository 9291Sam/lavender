#pragma once

#include "gfx/profiler/profiler.hpp"
#include <algorithm>
#include <chrono>
namespace gfx::profiler
{
    struct TaskGenerator
    {
        TaskGenerator()
            : start {std::chrono::steady_clock::now()}
            , previous_stamp_end {this->start}
        {}

        void stamp(std::string name)
        {
            const std::chrono::time_point<std::chrono::steady_clock> now =
                std::chrono::steady_clock::now();

            this->tasks.push_back(ProfilerTask {
                .start_time {
                    std::chrono::duration<float> {this->previous_stamp_end - this->start}.count()},
                .end_time {std::chrono::duration<float> {now - this->start}.count()},
                .name {std::move(name)},
                .color {
                    gfx::profiler::Colors.at(this->tasks.size() % gfx::profiler::Colors.size())}});

            this->previous_stamp_end = now;
        }

        [[nodiscard]] std::vector<ProfilerTask> getTasks() const
        {
            return this->tasks;
        }

    private:
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::chrono::time_point<std::chrono::steady_clock> previous_stamp_end;
        std::vector<ProfilerTask>                          tasks;
    };
} // namespace gfx::profiler