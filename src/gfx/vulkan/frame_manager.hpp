#pragma once

#include <chrono>
#include <cstddef>

namespace gfx::vulkan
{
    constexpr std::size_t MaxFramesInFlight = 3;

    constexpr std::size_t TimeoutNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<std::size_t> {5})
            .count();

} // namespace gfx::vulkan