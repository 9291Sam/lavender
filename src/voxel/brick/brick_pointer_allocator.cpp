#include "brick_allocator.hpp"
#include "util/index_allocator.hpp"
#include "voxel/brick/brick_pointer.hpp"
#include <utility>

namespace voxel::brick
{
    BrickPointerAllocator::BrickPointerAllocator(u32 maxBricks)
        : allocator {maxBricks}
    {}

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
} // namespace voxel::brick