#pragma once

#include "game/transform.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"

namespace verdigris
{
    struct TriangleComponent
    {
        game::Transform transform;
    };

    struct VoxelComponent
    {
        voxel::Voxel         voxel;
        voxel::WorldPosition position;
    };
} // namespace verdigris
