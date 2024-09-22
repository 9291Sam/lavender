#pragma once

#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include <array>

namespace voxel::data
{
    struct MaterialBrick
    {
        std::array<std::array<std::array<Voxel, 8>, 8>, 8> data;
    };
} // namespace voxel::data
