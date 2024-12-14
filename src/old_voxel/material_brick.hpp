#pragma once

#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include <array>

namespace old_voxel
{
    struct MaterialBrick
    {
        void fill(Voxel v)
        {
            for (auto& yz : this->data)
            {
                for (auto& z : yz)
                {
                    for (Voxel& voxel : z)
                    {
                        voxel = v;
                    }
                }
            }
        }

        void iterateOverVoxels(std::invocable<BrickLocalPosition, Voxel> auto func) const
        {
            for (int x = 0; x < 8; ++x)
            {
                for (int y = 0; y < 8; ++y)
                {
                    for (int z = 0; z < 8; ++z)
                    {
                        func(
                            BrickLocalPosition {glm::u8vec3 {x, y, z}},
                            this->data[x][y][z]); // NOLINT
                    }
                }
            }
        }

        [[nodiscard]] Voxel read(BrickLocalPosition p) const
        {
            return this->data[p.x][p.y][p.z];
        }

        std::array<std::array<std::array<Voxel, 8>, 8>, 8> data;
    };
} // namespace old_voxel
