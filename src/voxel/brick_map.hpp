#pragma once

#include "brick_pointer.hpp"
#include "voxel/constants.hpp"
#include <array>

namespace voxel
{
    struct BrickMap
    {
        u32                                                            offset = 0;
        std::array<std::array<std::array<MaybeBrickPointer, 8>, 8>, 8> data;

        void iterateOverPointers(std::invocable<BrickCoordinate, MaybeBrickPointer> auto func) const
        {
            for (std::size_t x = 0; x < 8; ++x)
            {
                for (std::size_t y = 0; y < 8; ++y)
                {
                    for (std::size_t z = 0; z < 8; ++z)
                    {
                        func(
                            BrickCoordinate {glm::u8vec3 {x, y, z}},
                            this->data[x][y][z]); // NOLINT
                    }
                }
            }
        }
    };
} // namespace voxel
