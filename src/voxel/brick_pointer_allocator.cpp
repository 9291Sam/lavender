#include "brick_pointer_allocator.hpp"
#include "brick_pointer.hpp"
#include "util/index_allocator.hpp"
#include <type_traits>
#include <utility>

namespace voxel
{
    BrickPointerAllocator::BrickPointerAllocator(u32 maxBricks)
        : allocator {maxBricks}
    {}

    BrickPointerAllocator::~BrickPointerAllocator() = default;

    BrickPointer BrickPointerAllocator::allocate()
    {
        const std::expected<u32, util::IndexAllocator::OutOfBlocks> newPointer =
            this->allocator.allocate();

        if (!newPointer.has_value())
        {
            util::panic("Allocation of new brick pointer failed!");
            std::unreachable();
        }
        else
        {
            return BrickPointer {*newPointer};
        }
    }

    void BrickPointerAllocator::free(BrickPointer pointer)
    {
        this->allocator.free(pointer.pointer);
    }
} // namespace voxel
