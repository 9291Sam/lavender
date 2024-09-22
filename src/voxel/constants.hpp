#pragma once

#include "util/misc.hpp"
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>

namespace voxel
{
    static constexpr u8 ChunkEdgeLengthVoxels = 64;
    static constexpr u8 ChunkEdgeLengthBricks = 8;
    static constexpr u8 BrickEdgeLengthVoxels = 8;

    struct BrickLocalPosition : public glm::u8vec3
    {};

    struct BrickCoordinate : public glm::u8vec3
    {};

    struct ChunkLocalPosition : public glm::u8vec3
    {};

    inline std::pair<BrickCoordinate, BrickLocalPosition>
    splitChunkLocalPosition(ChunkLocalPosition p)
    {
        return {
            BrickCoordinate {p / ChunkEdgeLengthBricks},
            BrickLocalPosition {p % ChunkEdgeLengthBricks}};
    }
} // namespace voxel
