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
        std::vector<float> res {};
        res.resize(64 * 64);

        std::vector<float> mat {};
        mat.resize(64 * 64);

        this->fractal->GenUniformGrid2D(
            res.data(), root.z, root.x, 64, 64, 0.02f, static_cast<int>(this->seed * 13437));
        this->fractal->GenUniformGrid2D(
            mat.data(), root.z, root.x, 64, 64, 0.02f, static_cast<int>(this->seed * 8594835));
        const std::array<std::array<float, 64>, 64>* ptr = // NOLINTNEXTLINE
            reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(res.data());

        const std::array<std::array<float, 64>, 64>* matptr = // NOLINTNEXTLINE
            reinterpret_cast<const std::array<std::array<float, 64>, 64>*>(mat.data());

        std::vector<voxel::ChunkLocalUpdate> out {};
        out.reserve(4096 * 8);

        for (std::size_t i = 0; i < 64; ++i)
        {
            for (std::size_t j = 0; j < 64; ++j)
            {
                u8 height = static_cast<u8>(
                    std::clamp((4.0f * (*ptr)[i][j]) + 8.0f, 0.0f, 64.0f)); // NOLINT

                for (u8 h = 0; h < height; ++h)
                {
                    out.push_back(voxel::ChunkLocalUpdate {
                        voxel::ChunkLocalPosition {{i, h, j}},
                        static_cast<voxel::Voxel>(
                            util::map<float>((*matptr)[i][j], -1.0f, 1.0f, 1.0f, 11.9f)),
                        voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                        voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
                }
            }
        }

        return out;
    }
} // namespace world