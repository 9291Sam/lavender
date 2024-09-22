#pragma once

#include "brick_pointer.hpp"
#include <array>

namespace voxel::brick
{
    struct BrickMap
    {
        std::array<std::array<std::array<MaybeBrickPointer, 8>, 8>, 8> data;
    };
} // namespace voxel::brick
