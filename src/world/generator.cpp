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
        std::array<std::array<float, 64>, 64> res {};

        std::unique_ptr<std::array<std::array<std::array<float, 64>, 64>, 64>> mat {
            new std::array<std::array<std::array<float, 64>, 64>, 64>};
        this->fractal->GenUniformGrid2D(
            res.data()->data(),
            root.x,
            root.z,
            64,
            64,
            0.02f,
            static_cast<int>(this->seed * 13437));
        this->fractal->GenUniformGrid3D(
            mat->data()->data()->data(),
            root.x,
            root.z,
            root.y,
            64,
            64,
            64,
            0.05f,
            static_cast<int>(this->seed * 8594835));

        std::vector<voxel::ChunkLocalUpdate> out {};
        out.reserve(32768);

        for (u8 i = 0; i < 64; ++i)
        {
            for (u8 j = 0; j < 64; ++j)
            {
                const i32 worldHeight = static_cast<i32>(std::exp(4.0f * res[j][i])); // NOLINT

                for (u8 h = 0; h < 64; ++h)
                {
                    const i32 worldHeightThisVoxel = h + root.y;
                    if (worldHeightThisVoxel < worldHeight)
                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            static_cast<voxel::Voxel>(util::map<float>(
                                static_cast<float>((*mat)[h][j][i]),
                                -1.0f,
                                1.0f,
                                1.0f,
                                17.9f)), // NOLINT
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                }
            }
        }

        return out;
    }
} // namespace world