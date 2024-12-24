#include "generator.hpp"
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
    WorldGenerator::generateChunk(voxel::WorldPosition root) const
    {
        if (root.x == 0 && root.z == 0)
        {
            return {};
        }

        auto gen2D =
            [&](float       scale,
                std::size_t localSeed) -> std::unique_ptr<std::array<std::array<float, 64>, 64>>
        {
            std::unique_ptr<std::array<std::array<float, 64>, 64>> res {
                new std::array<std::array<float, 64>, 64>};

            this->fractal->GenUniformGrid2D(
                res->data()->data(), root.x, root.z, 64, 64, scale, static_cast<int>(localSeed));

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

        auto height      = gen2D(0.001f, this->seed + 487484);
        auto bumpHeight  = gen2D(0.01f, this->seed + 7373834);
        auto mainRock    = gen3D(0.001f, this->seed - 747875);
        auto pebblesRock = gen3D(0.01f, this->seed - 52649274);
        auto pebbles     = gen3D(0.05f, this->seed - 948);

        std::vector<voxel::ChunkLocalUpdate> out {};
        out.reserve(32768);

        for (u8 j = 0; j < 64; ++j)
        {
            for (u8 i = 0; i < 64; ++i)
            {
                const i32 worldHeight =
                    static_cast<i32>((*height)[j][i] * 64.0f + (*bumpHeight)[j][i] * 2.0f);

                for (u8 h = 0; h < 64; ++h)
                {
                    const i32 worldHeightThisVoxel     = h + root.y;
                    const i32 relativeDistanceToHeight = worldHeightThisVoxel - worldHeight;

                    if (relativeDistanceToHeight < 0)
                    {
                        std::array<std::array<std::array<float, 64>, 64>, 64>* rockSampler =
                            (*pebbles)[h][j][i] > 0.75f ? pebblesRock.get() : mainRock.get();

                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            static_cast<voxel::Voxel>(util::map<float>(
                                static_cast<float>((*rockSampler)[h][j][i]),
                                -1.0f,
                                1.0f,
                                14.0f,
                                18.0f)), // NOLINT
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                    else if (relativeDistanceToHeight < 2)
                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            voxel::Voxel::Dirt,
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                    else if (relativeDistanceToHeight < 3)
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