#ifndef SRC_UTIL_BLOCK_ALLOCATOR_HPP
#define SRC_UTIL_BLOCK_ALLOCATOR_HPP

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
        IndexAllocator(IndexAllocator&&)                  = delete;
        IndexAllocator& operator= (const IndexAllocator&) = delete;
        IndexAllocator& operator= (IndexAllocator&&)      = delete;

        void updateAvailableBlockAmount(IndexType newAmount);
        // TODO: float getPercentAllocated() const;

        std::expected<IndexType, OutOfBlocks> allocate();
        void                                  free(IndexType);

    private:
        boost::container::flat_set<IndexType> free_block_list;
        IndexType                             next_available_block;
        IndexType                             max_number_of_blocks;
    };
} // namespace util

#endif // SRC_UTIL_BLOCK_ALLOCATOR_HPP