#include "world_manager.hpp"
#include "game/camera.hpp"
#include "game/game.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"
#include <random>

namespace voxel
{
    WorldManager::WorldManager(const game::Game* game)
        : chunk_render_manager {game}
    {
        this->chunks = std::async(
            [this]
            {
                world::WorldGenerator gen {789123};

                const std::int64_t radius = 4;

                std::vector<std::unique_ptr<voxel::ChunkRenderManager::Chunk>> threadChunks {};
                for (int x = -radius; x <= radius; ++x)
                {
                    for (int y = -1; y <= 0; ++y)
                    {
                        for (int z = -radius; z <= radius; ++z)
                        {
                            const voxel::WorldPosition pos {{64 * x, 64 * y, 64 * z}};

                            threadChunks.emplace_back(
                                std::make_unique<voxel::ChunkRenderManager::Chunk>(
                                    this->chunk_render_manager.createChunk(pos)));

                            std::vector<voxel::ChunkLocalUpdate> slowUpdates =
                                gen.generateChunk(pos);

                            this->updates.lock(
                                [&](std::vector<std::pair<
                                        const voxel::ChunkRenderManager::Chunk*,
                                        std::vector<voxel::ChunkLocalUpdate>>>& lockedUpdates)
                                {
                                    lockedUpdates.push_back(
                                        {threadChunks.back().get(), std::move(slowUpdates)});
                                });
                        }
                    }
                }

                return threadChunks;
            });

        std::mt19937_64                     gen {73847375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};

        for (int i = 0; i < 128; ++i)
        {
            this->raytraced_lights.push_back(
                this->chunk_render_manager.createRaytracedLight(voxel::GpuRaytracedLight {
                    .position_and_half_intensity_distance {glm::vec4 {
                        util::map(dist(gen), 0.0f, 1.0f, -384.0f, 384.0f) + 32.0f,
                        util::map(dist(gen), 0.0f, 1.0f, 64.0f, 184.0f),
                        util::map(dist(gen), 0.0f, 1.0f, -384.0f, 384.0f) + 32.0f,
                        16.0f}},
                    .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), 96}}}));
        }
    }

    WorldManager::~WorldManager()
    {
        for (std::unique_ptr<voxel::ChunkRenderManager::Chunk>& c : this->chunks.get())
        {
            this->chunk_render_manager.destroyChunk(std::move(*c));
        }

        for (voxel::ChunkRenderManager::RaytracedLight& l : this->raytraced_lights)
        {
            this->chunk_render_manager.destroyRaytracedLight(std::move(l));
        }
    }

    std::vector<game::FrameGenerator::RecordObject>
    WorldManager::onFrameUpdate(const game::Camera& camera)
    {
        this->updates.lock(
            [&](std::vector<std::pair<
                    const voxel::ChunkRenderManager::Chunk*,
                    std::vector<voxel::ChunkLocalUpdate>>>& lockedUpdates)
            {
                int numberOfUpdates = 0;

                while (!lockedUpdates.empty() && numberOfUpdates++ < 1)
                {
                    const auto it                       = lockedUpdates.crbegin();
                    const auto [chunkPtr, localUpdates] = *it;

                    this->chunk_render_manager.updateChunk(*chunkPtr, localUpdates);

                    lockedUpdates.pop_back();
                }
            });

        return this->chunk_render_manager.processUpdatesAndGetDrawObjects(camera);
    }
} // namespace voxel