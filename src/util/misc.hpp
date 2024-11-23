#pragma once

#include "timer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <queue>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

using f32 = float;
using f64 = double;

namespace util
{
    std::size_t getMemoryUsage();

    template<class T>
    using Fn = T;

    [[noreturn]] void debugBreak(std::source_location = std::source_location::current());

    consteval bool isDebugBuild()
    {
        return LAVENDER_DEBUG_BUILD;
    }

    template<class T>
    constexpr std::underlying_type_t<T> toUnderlying(T t) noexcept
    {
        return static_cast<std::underlying_type_t<T>>(t);
    }

    constexpr inline std::string formCallingLocation(std::source_location location)
    {
        constexpr std::array<std::string_view, 2> FolderIdentifiers {"/src/", "/inc/"};
        std::string                               rawFileName {location.file_name()};
        for (char& c : rawFileName)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }

        std::size_t index = std::string::npos; // NOLINT

        for (const std::string_view str : FolderIdentifiers)
        {
            if (index != std::string::npos)
            {
                break;
            }

            index = rawFileName.find(str);
        }

        return std::format(
            "{}:{}:{}", rawFileName.substr(index + 1), location.line(), location.column());
    }

    constexpr inline void hashCombine(std::size_t& seed_, std::size_t hash_) noexcept
    {
        hash_ += 0x9e3779b9 + (seed_ << 6) + (seed_ >> 2);
        seed_ ^= hash_;
    }

    template<class T>
    struct ZSTTypeWrapper
    {};

    template<class I>
    I divideEuclidean(I lhs, I rhs) // NOLINT
    {
        const I quotient = lhs / rhs;

        if (lhs % rhs < 0)
        {
            return rhs > 0 ? quotient - 1 : quotient + 1;
        }

        return quotient;
    }

    template<class I>
    I moduloEuclidean(I lhs, I rhs)
    {
        const I remainder = lhs % rhs;

        if (remainder < 0)
        {
            return rhs > 0 ? remainder + rhs : remainder - rhs;
        }

        return remainder;
    }

    template<class T>

    [[nodiscard]] T map(T x, T inMin, T inMax, T outMin, T outMax)
    {
        return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    }

    // inclusive indicies
    std::vector<std::pair<size_t, size_t>>
    combineIntoRanges(std::vector<size_t> points, size_t maxDistance, size_t maxRanges);

    enum class SuffixType
    {
        Short,
        Full
    };

    std::string bytesAsSiNamed(std::size_t, SuffixType = SuffixType::Full);
    std::string bytesAsSiNamed(long double bytes, SuffixType = SuffixType::Full);
} // namespace util
