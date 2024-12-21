#include "world_manager.hpp"
#include "game/camera.hpp"
#include "game/game.hpp"
#include "structures.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <vector>

namespace voxel
{
    static constexpr std::size_t MaxVoxelObjects = 4096;

    WorldManager::WorldManager(const game::Game* game)
        : chunk_render_manager {game}
        , world_generator {3347347348}
        , voxel_object_allocator {MaxVoxelObjects}
    {
        this->voxel_object_tracking_data.resize(MaxVoxelObjects, VoxelObjectTrackingData {});

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
        for (auto& [pos, c] : this->chunks)
        {
            this->chunk_render_manager.destroyChunk(std::move(c));
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

                util::assertWarn(
                    lockedUpdates.empty(),
                    "Too many chunk updates! {} remaining",
                    lockedUpdates.size());

                const i32 radius                = 4;
                const auto [cameraChunkBase, _] = splitWorldPosition(
                    WorldPosition {static_cast<glm::i32vec3>(camera.getPosition())});
                const glm::i32vec3 chunkRangeBase = cameraChunkBase - radius;
                const glm::i32vec3 chunkRangePeak = cameraChunkBase + radius;

                for (int x = chunkRangeBase.x; x <= chunkRangePeak.x; ++x)
                {
                    for (int y = chunkRangeBase.y; y <= chunkRangePeak.y; ++y)
                    {
                        for (int z = chunkRangeBase.z; z <= chunkRangePeak.z; ++z)
                        {
                            const voxel::WorldPosition pos {{(64 * x), (64 * y), (64 * z)}};

                            const decltype(this->chunks)::const_iterator maybeIt =
                                this->chunks.find(pos);

                            if (maybeIt == this->chunks.cend())
                            {
                                const auto [realIt, _] = this->chunks.insert(
                                    {pos, this->chunk_render_manager.createChunk(pos)});

                                std::vector<voxel::ChunkLocalUpdate> slowUpdates =
                                    this->world_generator.generateChunk(pos);

                                lockedUpdates.push_back({&realIt->second, std::move(slowUpdates)});

                                goto exit;
                            }
                        }
                    }
                }

                std::erase_if(
                    this->chunks,
                    [&](decltype(this->chunks)::value_type& item)
                    {
                        auto& [pos, chunk] = item;

                        if (glm::all(glm::lessThanEqual(pos, chunkRangePeak * 64))
                            && glm::all(glm::greaterThanEqual(pos, chunkRangeBase * 64)))
                        {
                            return false;
                        }
                        else
                        {
                            this->chunk_render_manager.destroyChunk(std::move(chunk));

                            return true;
                        }
                    });

            exit:
            });

        return this->chunk_render_manager.processUpdatesAndGetDrawObjects(camera);
    }
} // namespace voxel