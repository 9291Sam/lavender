#include "generator.hpp"
#include "shaders/include/common.glsl"
#include "voxel/structures.hpp"

namespace world
{
    WorldGenerator::WorldGenerator(u64 seed_)
        : simplex {FastNoise::New<FastNoise::Simplex>()}
        , fractal {FastNoise::New<FastNoise::FractalFBm>()}
        , seed {seed_}
    {
        this->fractal->SetSource(this->simplex);
        this->fractal->SetOctaveCount(1);
        this->fractal->SetGain(1.024f);
        this->fractal->SetLacunarity(2.534f);
    }

    std::vector<voxel::ChunkLocalUpdate>
    WorldGenerator::generateChunk(voxel::ChunkLocation chunkRoot) const
    {
        const voxel::WorldPosition root {chunkRoot.root_position};

        // if (root.x == 0 && root.z == 0)
        // {
        //     return {};
        // }

        const i32 integerScale = static_cast<i32>(gpu_calculateChunkVoxelSizeUnits(chunkRoot.lod));

        auto gen2D =
            [&](float       scale,
                std::size_t localSeed) -> std::unique_ptr<std::array<std::array<float, 64>, 64>>
        {
            std::unique_ptr<std::array<std::array<float, 64>, 64>> res {
                new std::array<std::array<float, 64>, 64>};

            this->fractal->GenUniformGrid2D(
                res->data()->data(),
                root.x / integerScale,
                root.z / integerScale,
                64,
                64,
                scale,
                static_cast<int>(localSeed));

            return res;
        };

        auto gen3D = [&](float scale, std::size_t localSeed)
            -> std::unique_ptr<std::array<std::array<std::array<float, 64>, 64>, 64>>
        {
            std::unique_ptr<std::array<std::array<std::array<float, 64>, 64>, 64>> res {
                new std::array<std::array<std::array<float, 64>, 64>, 64>};

            this->fractal->GenUniformGrid3D(
                res->data()->data()->data(),
                root.x,
                root.z,
                root.y,
                64,
                64,
                64,
                scale,
                static_cast<int>(localSeed));

            return res;
        };

        auto height     = gen2D(static_cast<float>(integerScale) * 0.001f, this->seed + 487484);
        auto bumpHeight = gen2D(static_cast<float>(integerScale) * 0.01f, this->seed + 7373834);
        auto mountainHeight =
            gen2D(static_cast<float>(integerScale) * 1.0f / 16384.0f, (this->seed * 3884) - 83483);
        // auto mainRock    = gen3D(static_cast<float>(integerScale) * 0.001f, this->seed - 747875);
        // auto pebblesRock = gen3D(static_cast<float>(integerScale) * 0.01f, this->seed -
        // 52649274); auto pebbles     = gen3D(static_cast<float>(integerScale) * 0.05f, this->seed
        // - 948);

        std::vector<voxel::ChunkLocalUpdate> out {};
        out.reserve(32768);

        for (u8 j = 0; j < 64; ++j)
        {
            for (u8 i = 0; i < 64; ++i)
            {
                // const i32 unscaledWorldHeight =
                //     static_cast<i32>(std::exp2((*height)[j][i] * 12.0f));

                const i32 unscaledWorldHeight = static_cast<i32>(
                    (*height)[j][i] * 32.0f + (*bumpHeight)[j][i] * 2.0f
                    + (*mountainHeight)[j][i] * 1024.0f);

                for (u8 h = 0; h < 64; ++h)
                {
                    const i32 worldHeightOfVoxel =
                        static_cast<i32>(h * gpu_calculateChunkVoxelSizeUnits(chunkRoot.lod))
                        + root.y;
                    const i32 relativeDistanceToHeight =
                        (worldHeightOfVoxel - unscaledWorldHeight) + (4 * integerScale);

                    if (relativeDistanceToHeight < 0 * integerScale)
                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            static_cast<voxel::Voxel>(util::map<float>(
                                0.76f,
                                -1.0f,
                                1.0f,
                                14.0f,
                                18.0f)), // NOLINT
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                    else if (relativeDistanceToHeight < 2 * integerScale)
                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            voxel::Voxel::Dirt,
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                    else if (relativeDistanceToHeight < 3 * integerScale)

                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            voxel::Voxel::Grass,
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                }
            }
        }

        return out;
    }
} // namespace world

// rolling hills
// a few mountains
// simple ores
// marble
