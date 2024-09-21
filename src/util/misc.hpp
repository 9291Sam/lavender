#pragma once

#include <cstdint>
#include <type_traits>

using u8  = unsigned char; // for aliasing rules
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
// using U128 = __uint128_t;

using i8  = signed char;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
// using I128 = __int128_t;

using F32 = float;
using F64 = double;

namespace util
{
    template<class T>
    using Fn = T;

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

    template<class T>
    struct ZSTTypeWrapper
    {};

} // namespace util
