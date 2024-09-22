#pragma once

#include "voxel/voxel.hpp"
#include <array>

namespace voxel::data
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

        std::array<std::array<std::array<Voxel, 8>, 8>, 8> data;
    };
} // namespace voxel::data
