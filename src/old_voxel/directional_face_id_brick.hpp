#pragma once

#include "voxel/brick_pointer.hpp"
#include <array>

namespace old_voxel
{
    struct FaceIdBrick
    {
        std::array<std::array<std::array<u32, 8>, 8>, 8> data;
    };

    struct DirectionalFaceIdBrick
    {
        std::array<FaceIdBrick, 6> brick_directions;
    };
} // namespace old_voxel
