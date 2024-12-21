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

        std::array<std::array<float, 64>, 64> mat {};
        this->fractal->GenUniformGrid2D(
            res.data()->data(),
            root.z,
            root.x,
            64,
            64,
            0.02f,
            static_cast<int>(this->seed * 13437));
        this->fractal->GenUniformGrid2D(
            mat.data()->data(),
            root.z,
            root.x,
            64,
            64,
            0.02f,
            static_cast<int>(this->seed * 8594835));

        std::vector<voxel::ChunkLocalUpdate> out {};
        out.reserve(4096 * 8);

        for (i32 i = 0; i < 64; ++i)
        {
            for (i32 j = 0; j < 64; ++j)
            {
                const float worldHeight = std::exp(4.0f * res[i][j]); // NOLINT

                for (u8 h = 0; h < 64; ++h)
                {
                    if (h + root.y < worldHeight)
                    {
                        out.push_back(voxel::ChunkLocalUpdate {
                            voxel::ChunkLocalPosition {{i, h, j}},
                            static_cast<voxel::Voxel>(
                                util::map<float>(mat[i][j], -1.0f, 1.0f, 1.0f, 17.9f)),
                            voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                            voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                    }
                }
            }
        }

        return out;
    }
} // namespace world