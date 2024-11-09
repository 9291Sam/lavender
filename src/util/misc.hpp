#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>
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

using F32 = float;
using F64 = double;

namespace util
{
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
        const std::string                         rawFileName {location.file_name()};

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
    inline std::vector<std::pair<size_t, size_t>>
    combineIntoRanges(std::vector<size_t> points, size_t maxDistance, size_t maxRanges)
    {
        if (points.empty())
        {
            return {};
        }

        // Sort the points
        std::sort(points.begin(), points.end());

        std::vector<std::pair<size_t, size_t>> ranges;
        ranges.reserve(points.size());

        size_t start = points[0];
        size_t end   = start;

        // Create initial ranges based on maxDistance
        for (size_t i = 1; i < points.size(); ++i)
        {
            size_t point = points[i];
            if (point - end <= maxDistance)
            {
                end = point;
            }
            else
            {
                ranges.emplace_back(start, end);
                start = point;
                end   = point;
            }
        }

        // Push the last range
        ranges.emplace_back(start, end);

        // Merge ranges if they exceed maxRanges
        if (ranges.size() > maxRanges)
        {
            while (ranges.size() > maxRanges)
            {
                size_t minDistance = std::numeric_limits<size_t>::max();
                size_t minIndex    = 0;

                // Find the closest adjacent ranges
                for (size_t i = 0; i < ranges.size() - 1; ++i)
                {
                    size_t distance = ranges[i + 1].first - ranges[i].second;
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        minIndex    = i;
                    }
                }

                // Merge the closest ranges
                ranges[minIndex].second = ranges[minIndex + 1].second;
                ranges.erase(
                    ranges.begin() + static_cast<std::vector<size_t>::difference_type>(minIndex)
                    + 1);
            }

            // Resize to maxRanges
            ranges.resize(maxRanges);
        }

        return ranges;
    }
} // namespace util
