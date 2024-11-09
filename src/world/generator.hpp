#pragma once

#include "util/misc.hpp"
#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"
#include "voxel/world.hpp"
#include <FastNoise/FastNoise.h>
#include <boost/container/detail/destroyers.hpp>

namespace world
{
    struct WorldChunkGenerator : public voxel::World::ChunkGenerator
    {
    public:
        WorldChunkGenerator(std::size_t seed)
            : simplex {FastNoise::New<FastNoise::Simplex>()}
            , fractal {FastNoise::New<FastNoise::FractalFBm>()}
            , seed {seed}
        {
            this->fractal->SetSource(this->simplex);
            this->fractal->SetOctaveCount(5);
            this->fractal->SetGain(1.024);
            this->fractal->SetLacunarity(4.034);
            this->fractal->SetWeightedStrength(9.403);
        }

        ~WorldChunkGenerator() override = default;

        std::vector<voxel::World::VoxelWrite>
        generateChunk(voxel::ChunkCoordinate coordinate) override
        {
            if (coordinate.y != 0)
            {
                return {};
            }
            voxel::WorldPosition root = voxel::getWorldPositionOfChunkCoordinate(coordinate);

            std::vector<float> res {};
            res.resize(64 * 64);

            std::vector<float> mat {};
            mat.resize(64 * 64);

            this->simplex->GenUniformGrid2D(
                res.data(), root.z, root.x, 64, 64, 0.02f, this->seed * 13437);
            this->simplex->GenUniformGrid2D(
                mat.data(), root.z, root.x, 64, 64, 0.02f, this->seed * 8594835);
            const std::array<std::array<float, 64>, 64>* ptr =
                reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(res.data());

            const std::array<std::array<float, 64>, 64>* matptr =
                reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(mat.data());

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
                            .voxel {static_cast<voxel::Voxel>(
                                util::map<float>((*matptr)[i][j], -1.0f, 1.0f, 1.0f, 9.0f))}};

                        out.push_back(w);
                    }
                }
            }

            return out;
        }

    private:
        FastNoise::SmartNode<FastNoise::Simplex>    simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm> fractal;
        std::size_t                                 seed;
    };
} // namespace world