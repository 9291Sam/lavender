#pragma once

#include "util/misc.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include "voxel/world.hpp"
#include <FastNoise/FastNoise.h>

namespace world
{
    struct WorldChunkGenerator
    {
    public:
        WorldChunkGenerator(std::size_t seed)
            : simplex {FastNoise::New<FastNoise::Simplex>()}
            , fractal {FastNoise::New<FastNoise::FractalFBm>()}
        {
            this->fractal->SetSource(this->simplex);
            this->fractal->SetOctaveCount(5);
        }

        std::vector<voxel::World::VoxelWrite> generateChunk(voxel::ChunkCoordinate coordinate)
        {
            voxel::WorldPosition root = voxel::getWorldPositionOfChunkCoordinate(coordinate);

            std::vector<float> res {};
            res.resize(64 * 64);

            this->simplex->GenUniformGrid2D(res.data(), root.z, root.x, 64, 64, 0.002f, 13437);

            const std::array<std::array<float, 64>, 64>* ptr =
                reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(res.data());

            std::vector<voxel::World::VoxelWrite> out {};
            out.reserve(4096);

            for (int i = 0; i < 64; ++i)
            {
                for (int j = 0; j < 64; ++j)
                {
                    u8 height = 12 * (*ptr)[i][j] + 16;
                    height    = std::clamp(
                        height, u8 {1}, static_cast<u8>(voxel::ChunkEdgeLengthVoxels - 1));

                    for (u8 h = 0; h < height; ++h)
                    {
                        auto w = voxel::World::VoxelWrite {
                            .position {voxel::assembleWorldPosition(
                                coordinate, voxel::ChunkLocalPosition {{i, h, j}})},
                            .voxel {voxel::Voxel::Obsidian}};

                        out.push_back(w);
                    }
                }
            }

            return out;
        }

    private:
        FastNoise::SmartNode<FastNoise::Simplex>    simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm> fractal;
    };
} // namespace world