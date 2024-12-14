#pragma once

#include "util/log.hpp"
#include <compare>
#include <cstring>
#include <type_traits>

namespace util
{
    template<std::size_t N>
    struct StringSneaker
    {
        constexpr StringSneaker(const char (&string)[N]) // NOLINT: We want implicit
            : data {std::to_array<const char>(string)}
        {}

        [[nodiscard]]
        constexpr std::string_view getStringView() const noexcept
        {
            return std::string_view {this->data.data(), N - 1};
        }

        std::array<char, N> data;
    };

    template<StringSneaker Name, class I, class FriendClass, I NullValue = static_cast<I>(-1)>
        requires (std::is_integral_v<I> && !std::is_floating_point_v<I>)
    struct OpaqueHandle
    {
    public:
        static constexpr OpaqueHandle NullHandle {};
    public:
        constexpr OpaqueHandle()
            : value {NullValue}
        {}
        constexpr ~OpaqueHandle()
        {
            if (!this->isNull())
            {
                util::logWarn("Leak of {} with value {}", Name.getStringView(), this->value);
            }
        }

        OpaqueHandle(const OpaqueHandle&) = delete;
        OpaqueHandle(OpaqueHandle&& other) noexcept
            : value {other.value}
        {
            other.value = NullValue;
        }
        OpaqueHandle& operator= (const OpaqueHandle&) = delete;
        OpaqueHandle& operator= (OpaqueHandle&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~OpaqueHandle();

            new (this) OpaqueHandle {std::move(other)};

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->value == NullValue;
        }

        std::strong_ordering operator<=> (const OpaqueHandle& other) const
        {
            return this->value <=> other.value;
        }

    protected:
        constexpr explicit OpaqueHandle(I value_)
            : value {value_}
        {}

        [[nodiscard]] I release() noexcept
        {
            return std::exchange(this->value, NullValue);
        }

    private:
        I value;
    };

} // namespace util