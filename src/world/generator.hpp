#pragma once

#include "util/misc.hpp"
#include "voxel/structures.hpp"
#include <FastNoise/FastNoise.h>

namespace world
{

    class WorldGenerator
    {
    public:
        explicit WorldGenerator(u64 seed);

        [[nodiscard]] std::vector<voxel::ChunkLocalUpdate>
            generateChunk(voxel::WorldPosition) const;

    private:

        FastNoise::SmartNode<FastNoise::Simplex>    simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm> fractal;
        std::size_t                                 seed;
    };
} // namespace world