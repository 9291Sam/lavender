#pragma once

#include "util/misc.hpp"

namespace voxel
{
    enum class Voxel : u16
    {
        NullAirEmpty = 0,
        Emerald,
        Ruby,
        Pearl,
        Obsidian,
        Brass,
        Chrome,
        Copper,
        Gold
    };

    static_assert(Voxel {} == Voxel::NullAirEmpty);
} // namespace voxel
