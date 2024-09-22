#pragma once

#include "util/misc.hpp"

namespace voxel::brick
{
    struct MaybeBrickPointer
    {
        static constexpr u32 Null = static_cast<u32>(-1);

        [[nodiscard]] bool isNull() const
        {
            return this->pointer == Null;
        }

        u32 pointer = Null;
    };

    struct BrickPointer : MaybeBrickPointer
    {};
} // namespace voxel::brick
