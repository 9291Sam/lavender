#pragma once

#include "misc.hpp"
#include <boost/container/flat_set.hpp>
#include <cstddef>
#include <expected>
#include <source_location>
#include <util/log.hpp>

namespace util
{
    /// Allocates unique, single integers.
    /// Useful for allocating fixed sized chunks of memory
    class IndexAllocator
    {
    public:

        using IndexType = u32;

        class OutOfBlocks : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };

        class FreeOfUntrackedValue : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };

        class DoubleFree : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };

    public:

        // Creates a new IndexAllocator that can allocate blocks at indicies in
        // the range [0, blocks)
        // IndexAllocator(128) -> [0, 127]
        explicit IndexAllocator(IndexType blocks);
        ~IndexAllocator() = default;

        IndexAllocator(const IndexAllocator&)             = delete;
        IndexAllocator(IndexAllocator&&)                  = default;
        IndexAllocator& operator= (const IndexAllocator&) = delete;
        IndexAllocator& operator= (IndexAllocator&&)      = default;

        void              updateAvailableBlockAmount(IndexType newAmount);
        [[nodiscard]] u32 getNumberAllocated() const
        {
            return static_cast<u32>(this->next_available_block - this->free_block_list.size());
        }
        [[nodiscard]] float getPercentAllocated() const
        {
            return static_cast<float>(this->getNumberAllocated())
                 / static_cast<float>(this->max_number_of_blocks);
        }

        IndexType allocateOrPanic(std::source_location loc = std::source_location::current())
        {
            std::expected<IndexType, OutOfBlocks> maybeNewAllocation = this->allocate();

            if (!maybeNewAllocation.has_value())
            {
                util::panic<>("util::IndexAllocation::allocateOrPanic failed!", loc);
            }

            return *maybeNewAllocation;
        }

        std::expected<IndexType, OutOfBlocks> allocate();
        void                                  free(IndexType);

        void iterateThroughAllocatedElements(std::invocable<IndexType> auto func)
        {
            for (u32 i = 0; i < this->next_available_block; ++i)
            {
                if (!this->free_block_list.contains(i))
                {
                    func(i);
                }
            }
        }

        [[nodiscard]] IndexType getUpperBoundOnAllocatedElements() const
        {
            return this->next_available_block;
        }

    private:
        boost::container::flat_set<IndexType> free_block_list;
        IndexType                             next_available_block;
        IndexType                             max_number_of_blocks;
    };
} // namespace util
