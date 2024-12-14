#pragma once

#include "voxel/constants.hpp"
#include <type_traits>
namespace old_voxel
{
    struct GreedyVoxelFace
    {
        u32 x      : 6;
        u32 y      : 6;
        u32 z      : 6;
        u32 width  : 6;
        u32 height : 6;
        u32 pad    : 2;
    };
    static_assert(std::is_trivially_copyable_v<GreedyVoxelFace>);
    static_assert(ChunkEdgeLengthVoxels == 64);
} // namespace old_voxel
