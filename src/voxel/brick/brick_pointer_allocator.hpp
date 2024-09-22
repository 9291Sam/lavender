#pragma once

#include "brick_pointer.hpp"
#include "util/index_allocator.hpp"

namespace voxel::brick
{

    class BrickPointerAllocator
    {
    public:
        explicit BrickPointerAllocator(u32 maxBricks);
        ~BrickPointerAllocator();

        BrickPointerAllocator(const BrickPointerAllocator&) = delete;
        BrickPointerAllocator(BrickPointerAllocator&&)      = delete;
        BrickPointerAllocator&
        operator= (const BrickPointerAllocator&)                   = delete;
        BrickPointerAllocator& operator= (BrickPointerAllocator&&) = delete;

        BrickPointer allocate();
        void         free(BrickPointer);
    private:
        util::IndexAllocator allocator;
    };
} // namespace voxel::brick
