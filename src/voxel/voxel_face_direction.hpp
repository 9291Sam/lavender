#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include <glm/vec3.hpp>

namespace voxel
{
    enum class VoxelFaceDirection
    {
        Top    = 0,
        Bottom = 1,
        Left   = 2,
        Right  = 3,
        Front  = 4,
        Back   = 5
    };

    inline glm::i8vec3 getDirFromDirection(VoxelFaceDirection dir)
    {
        switch (dir)
        {
        case VoxelFaceDirection::Top:
            return glm::i8vec3 {0, 1, 0};
        case VoxelFaceDirection::Bottom:
            return glm::i8vec3 {0, -1, 0};
        case VoxelFaceDirection::Left:
            return glm::i8vec3 {-1, 0, 0};
        case VoxelFaceDirection::Right:
            return glm::i8vec3 {1, 0, 0};
        case VoxelFaceDirection::Front:
            return glm::i8vec3 {0, 0, -1};
        case VoxelFaceDirection::Back:
            return glm::i8vec3 {0, 0, 1};
        default:
            util::panic(
                "voxel::getDirFromDirection(VoxelFaceDirection) passed invalid "
                "direction | Value: {}",
                util::toUnderlying(dir));
        }
    }
} // namespace voxel
