#pragma once

#include "offsetAllocator.hpp"
#include "util/misc.hpp"

namespace util
{
    struct RangeAllocation
    {
        OffsetAllocator::Allocation internal_allocation;
    };

    // wrapper around sebbbi's offset allocator to make it actually rule of 5
    // compliant as well as fixing a bunch of idiotic design decisions
    class RangeAllocator
    {
    public:

    public:
        RangeAllocator(u32 size, u32 maxAllocations);
        ~RangeAllocator() = default;

        RangeAllocator(const RangeAllocator&) = delete;
        RangeAllocator(RangeAllocator&&) noexcept;
        RangeAllocator& operator= (const RangeAllocator&) = delete;
        RangeAllocator& operator= (RangeAllocator&&) noexcept;
    private:

        OffsetAllocator::Allocator internal_allocator;
    };
} // namespace util