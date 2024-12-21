#pragma once

#include "timer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <future>
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
    template<class I>
        requires std::is_signed_v<I>
    inline I divideEuclidean(I lhs, I rhs)
    {
        int quotient  = lhs / rhs;
        int remainder = lhs % rhs;

        if (remainder != 0 && ((rhs < 0) != (lhs < 0)))
        {
            quotient -= 1;
        }

        return quotient;
    }

    template<class I>
        requires std::is_signed_v<I>
    I moduloEuclidean(I lhs, I rhs)
    {
        if (lhs < 0)
        {
            // Offset lhs to ensure positive remainder
            lhs += (-lhs / rhs + 1) * rhs;
        }

        const i32 remainder = lhs % rhs;
        if (remainder < 0)
        {
            return rhs > 0 ? remainder + rhs : remainder - rhs;
        }
        return remainder;
    }

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

    inline std::string formCallingLocation(std::source_location location)
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

    template<class T>
        requires std::is_move_constructible_v<T> && std::is_move_assignable_v<T>
    struct Lazy
    {
        explicit Lazy(std::future<T> future_)
            : future {std::move(future_)}
        {}
        ~Lazy();

        Lazy(const Lazy&)             = delete;
        Lazy(Lazy&&)                  = default;
        Lazy& operator= (const Lazy&) = delete;
        Lazy& operator= (Lazy&&)      = default;

        std::optional<T&> access()
        {
            this->propagateUpdates();

            if (this->maybe_t != nullptr)
            {
                return std::ref(*this->maybe_t);
            }
            else
            {
                return std::nullopt;
            }
        }
        std::optional<T> tryMove()
        {
            this->propagateUpdates();

            if (this->maybe_t != nullptr)
            {
                return std::move(*this->maybe_t);
            }
            else
            {
                return std::nullopt;
            }
        }

        T move()
        {
            this->propagateUpdates(true);

            return std::move(*this->maybe_t);
        }

    private:

        void propagateUpdates(bool shouldWait = false)
        {
            if (this->future != nullptr && (shouldWait || this->future.valid()))
            {
                this->maybe_t = std::make_unique<T>(this->future.get());
            }
        }

        std::future<T>     future;
        std::unique_ptr<T> maybe_t;
    };
} // namespace util
