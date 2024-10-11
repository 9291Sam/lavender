#pragma once

#include "opacity_brick.hpp"
namespace voxel
{
    struct VisibilityBrick
    {
        std::array<OpacityBrick, 6> brick_directions;
    };
} // namespace voxel
