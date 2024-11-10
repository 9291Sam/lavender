
#include "util/range_allocator.hpp"

// tldr this lib's cmake file is fucked and doesnt have header guards, this is
// the easiest solution
#include "offsetAllocator.cpp" // NOLINT: stupid fucking library
#include "util/log.hpp"
#include <source_location>

namespace util
{
    RangeAllocator::RangeAllocator(u32 size, u32 maxAllocations)
        : internal_allocator {std::make_unique<OffsetAllocator::Allocator>(size, maxAllocations)}
    {}

    RangeAllocator::~RangeAllocator() = default;

    RangeAllocation RangeAllocator::allocate(u32 size, std::source_location l)
    {
        std::expected<RangeAllocation, OutOfBlocks> result = this->tryAllocate(size);

        if (!result.has_value())
        {
            util::logTrace<>("RangeAllocator::OutOfBlocks", l);

            throw OutOfBlocks {};
        }

        return *result;
    }

    std::expected<RangeAllocation, RangeAllocator::OutOfBlocks>
    RangeAllocator::tryAllocate(u32 size)
    {
        OffsetAllocator::Allocation workingAllocation = this->internal_allocator->allocate(size);

        if (workingAllocation.offset == OffsetAllocator::Allocation::NO_SPACE)
        {
            return std::unexpected(OutOfBlocks {});
        }
        else
        {
            return RangeAllocation {
                .offset {workingAllocation.offset}, .metadata {workingAllocation.metadata}};
        }
    }

    [[nodiscard]] u32 RangeAllocator::getSizeOfAllocation(RangeAllocation allocation) const
    {
        return this->internal_allocator->allocationSize(OffsetAllocator::Allocation {
            .offset {allocation.offset}, .metadata {allocation.metadata}});
    }

    void RangeAllocator::free(RangeAllocation allocation)
    {
        this->internal_allocator->free(OffsetAllocator::Allocation {
            .offset {allocation.offset}, .metadata {allocation.metadata}});
    }

} // namespace util
