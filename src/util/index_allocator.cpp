#include "index_allocator.hpp"
#include <util/log.hpp>

namespace util
{
    const char* IndexAllocator::OutOfBlocks::what() const noexcept
    {
        return "IndexAllocator::OutOfBlocks";
    }

    const char* IndexAllocator::FreeOfUntrackedValue::what() const noexcept
    {
        return "IndexAllocator::FreeOfUntrackedValue";
    }

    const char* IndexAllocator::DoubleFree::what() const noexcept
    {
        return "IndexAllocator::DoubleFree";
    }

    IndexAllocator::IndexAllocator(IndexType blocks)
        : next_available_block {0}
        , max_number_of_blocks {blocks}
    {}

    void IndexAllocator::updateAvailableBlockAmount(IndexType newAmount)
    {
        util::assertFatal(
            newAmount > this->max_number_of_blocks,
            "Tried to update an allocator with less bricks!");

        this->max_number_of_blocks = newAmount;
    }

    std::expected<IndexAllocator::IndexType, IndexAllocator::OutOfBlocks> IndexAllocator::allocate()
    {
        if (this->free_block_list.empty())
        {
            const IndexType nextFreeBlock = this->next_available_block;

            ++this->next_available_block;

            if (this->next_available_block > this->max_number_of_blocks)
            {
                return std::unexpected(OutOfBlocks {});
            }

            return nextFreeBlock;
        }
        else
        {
            IndexType freeListBlock = *this->free_block_list.rbegin();

            this->free_block_list.erase(freeListBlock);

            if (freeListBlock >= this->max_number_of_blocks)
            {
                util::panic(
                    "Out of bounds block #{} was in freelist of allocator of "
                    "size #{}! Recursing!",
                    freeListBlock,
                    this->max_number_of_blocks);
            }

            return freeListBlock;
        }
    }

    void IndexAllocator::free(IndexType blockToFree)
    {
        if (blockToFree >= this->max_number_of_blocks)
        {
            throw FreeOfUntrackedValue {};
        }

        const auto& [maybeIterator, valid] = this->free_block_list.insert_unique(blockToFree);

        while (this->free_block_list.contains(this->next_available_block - 1))
        {
            this->free_block_list.erase(this->next_available_block - 1);
            --this->next_available_block;
        }

        if (!valid)
        {
            throw DoubleFree {};
        }
    }

} // namespace util
