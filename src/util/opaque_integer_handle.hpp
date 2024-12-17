#pragma once

#include "util/index_allocator.hpp"
#include "util/log.hpp"
#include <compare>
#include <cstring>
#include <source_location>
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

    template<class Handle>
    class OpaqueHandleAllocator;

    template<StringSneaker Name, class I, I NullValue = std::numeric_limits<I>::max()>
        requires (std::is_integral_v<I> && !std::is_floating_point_v<I>)
    struct OpaqueHandle final
    {
    public:
        using IndexType = I;

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
        static constexpr I Null = NullValue;

        constexpr explicit OpaqueHandle(I value_)
            : value {value_}
        {}

        [[nodiscard]] I release() noexcept
        {
            return std::exchange(this->value, NullValue);
        }

        friend OpaqueHandleAllocator<OpaqueHandle>;

    private:
        I value;
    };

    template<class Handle>
    class OpaqueHandleAllocator
    {
    public:
        explicit OpaqueHandleAllocator(Handle::IndexType numberOfElementsToAllocate)
            : allocator {static_cast<u32>(numberOfElementsToAllocate)}
        {}
        ~OpaqueHandleAllocator() = default;

        OpaqueHandleAllocator(const OpaqueHandleAllocator&)             = delete;
        OpaqueHandleAllocator(OpaqueHandleAllocator&&)                  = default;
        OpaqueHandleAllocator& operator= (const OpaqueHandleAllocator&) = delete;
        OpaqueHandleAllocator& operator= (OpaqueHandleAllocator&&)      = default;

        void updateAvailableBlockAmount(Handle::IndexType newAmount)
        {
            this->allocator.updateAvailableBlockAmount(newAmount);
        }

        [[nodiscard]] u32 getNumberAllocated() const
        {
            return this->allocator.getNumberAllocated();
        }

        [[nodiscard]] float getPercentAllocated() const
        {
            return this->getPercentAllocated();
        }

        [[nodiscard]] Handle
        allocateOrPanic(std::source_location loc = std::source_location::current())
        {
            return Handle {static_cast<Handle::IndexType>(this->allocator.allocateOrPanic(loc))};
        }

        [[nodiscard]] std::expected<Handle, IndexAllocator::OutOfBlocks> allocate()
        {
            return this->allocator.allocate().transform(
                [](const typename Handle::IndexType rawHandle)
                {
                    return Handle {rawHandle};
                });
        }

        void free(Handle handle)
        {
            this->allocator.free(handle.release());
        }

        [[nodiscard]] auto getValueOfHandle(const Handle& handle) const -> Handle::IndexType
        {
            return handle.value;
        }

        void iterateThroughAllocatedElements(std::invocable<typename Handle::IndexType> auto func)
        {
            this->allocator.iterateThroughAllocatedElements(
                [&](const util::IndexAllocator::IndexType i)
                {
                    func(static_cast<Handle::IndexType>(i));
                });
        }

        Handle::IndexType getUpperBoundOnAllocatedElements() const
        {
            return static_cast<Handle::IndexType>(
                this->allocator.getUpperBoundOnAllocatedElements());
        }

    private:
        util::IndexAllocator allocator;
    };
} // namespace util