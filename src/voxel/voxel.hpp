#pragma once

#include "util/misc.hpp"

namespace voxel
{
    enum class Voxel : u16
    {
        NullAirEmpty,
        Stone0,
        Stone1,
        Stone2,
        Stone3,
        Stone4,
        Stone6,
        Stone7,
        Dirt0,
        Dirt1,
        Dirt2,
        Dirt3,
        Dirt4,
        Dirt5,
        Dirt6,
        Dirt7
    };

    static_assert(Voxel {} == Voxel::NullAirEmpty);
} // namespace voxel
