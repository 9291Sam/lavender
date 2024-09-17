#pragma once

#include "misc.hpp"
#include <boost/container/flat_set.hpp>
#include <cstddef>
#include <expected>

namespace util
{
    /// Allocates unique, single integers.
    /// Useful for allocating fixed sized chunks of memory
    class IndexAllocator
    {
    public:

        using IndexType = U32;

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

        void updateAvailableBlockAmount(IndexType newAmount);
        // TODO: float getPercentAllocated() const;
        // NOT INCLUSIVE
        U32  getAllocatedBlocksUpperBound() const
        {
            return this->next_available_block;
        }

        std::expected<IndexType, OutOfBlocks> allocate();
        void                                  free(IndexType);

    private:
        boost::container::flat_set<IndexType> free_block_list;
        IndexType                             next_available_block;
        IndexType                             max_number_of_blocks;
    };
} // namespace util
