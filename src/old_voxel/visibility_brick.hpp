#pragma once

#include "opacity_brick.hpp"
namespace old_voxel
{
    struct VisibilityBrick
    {
        std::array<OpacityBrick, 6> brick_directions;
    };
} // namespace old_voxel
