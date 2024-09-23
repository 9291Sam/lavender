#include "misc.hpp"
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace util
{
    void debugBreak()
    {
        std::ignore = std::fputs("util::debugBreak()\n", stderr);

        std::this_thread::sleep_for(std::chrono::milliseconds {250});

        //         if constexpr (isDebugBuild())
        //         {
        // #ifdef _MSC_VER
        //             __debugbreak();
        // #elif defined(__GNUC__) || defined(__clang__)
        //             __builtin_trap();
        // #else
        // #error Unsupported compiler
        // #endif
        //         }

        throw std::runtime_error {"util::debugBreak()"};
    }

} // namespace util
