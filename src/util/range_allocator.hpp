#pragma once

#include "util/misc.hpp"
#include <expected>
#include <memory>

namespace OffsetAllocator // NOLINT stupid library
{
    class Allocator;
} // namespace OffsetAllocator

namespace util
{
    struct RangeAllocation
    {
        u32 offset;
        u32 metadata;
    };

    // sane wrapper around sebbbi's offset allocator to make it actually rule of
    // 5 compliant as well as fixing a bunch of idiotic design decisions (such
    // as not having header guards... and having a cmake script that screws
    // things up by default...)
    class RangeAllocator
    {
    public:
        class OutOfBlocks : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };
    public:
        RangeAllocator(u32 size, u32 maxAllocations);
        ~RangeAllocator() = default;

        RangeAllocator(const RangeAllocator&)                 = delete;
        RangeAllocator(RangeAllocator&&) noexcept             = default;
        RangeAllocator& operator= (const RangeAllocator&)     = delete;
        RangeAllocator& operator= (RangeAllocator&&) noexcept = default;

        [[nodiscard]] RangeAllocation allocate(u32 size);
        [[nodiscard]] std::expected<RangeAllocation, OutOfBlocks>
        tryAllocate(u32 size);

        void free(RangeAllocation);

    private:
        std::unique_ptr<OffsetAllocator::Allocator> internal_allocator;
    };
} // namespace util