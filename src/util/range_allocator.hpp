#pragma once

#include "util/misc.hpp"
#include <expected>
#include <memory>
#include <source_location>

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
        struct OutOfBlocks : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };
    public:
        RangeAllocator(u32 size, u32 maxAllocations);
        ~RangeAllocator();

        RangeAllocator(const RangeAllocator&) = delete;
        RangeAllocator(RangeAllocator&&) noexcept;
        RangeAllocator& operator= (const RangeAllocator&) = delete;
        RangeAllocator& operator= (RangeAllocator&&) noexcept;

        [[nodiscard]] RangeAllocation
        allocate(u32 size, std::source_location = std::source_location::current());
        [[nodiscard]] std::expected<RangeAllocation, OutOfBlocks> tryAllocate(u32 size);

        [[nodiscard]] u32                 getSizeOfAllocation(RangeAllocation) const;
        [[nodiscard]] std::pair<u32, u32> getStorageInfo() const;

        void free(RangeAllocation);

    private:
        std::unique_ptr<OffsetAllocator::Allocator> internal_allocator;
    };
} // namespace util
