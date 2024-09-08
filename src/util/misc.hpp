#pragma once

#include <type_traits>
namespace util
{
    static inline void debugBreak()
    {
        // TODO:
    }

    consteval bool isDebugBuild()
    {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }

} // namespace util
