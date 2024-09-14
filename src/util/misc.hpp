#pragma once

#include <cstdint>
#include <type_traits>

using U8   = std::uint8_t;
using U16  = std::uint16_t;
using U32  = std::uint32_t;
using U64  = std::uint64_t;
// using U128 = __uint128_t;

using I8   = std::int8_t;
using I16  = std::int16_t;
using I32  = std::int32_t;
using I64  = std::int64_t;
// using I128 = __int128_t;

using F32 = float;
using F64 = double;

namespace util
{
    void debugBreak();

    consteval bool isDebugBuild()
    {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }

    template<class T>
    constexpr std::underlying_type_t<T> toUnderlying(T t) noexcept
    {
        return static_cast<std::underlying_type_t<T>>(t);
    }

    constexpr inline void
    hashCombine(std::size_t& seed_, std::size_t hash_) noexcept
    {
        hash_ += 0x9e3779b9 + (seed_ << 6) + (seed_ >> 2);
        seed_ ^= hash_;
    }

} // namespace util
