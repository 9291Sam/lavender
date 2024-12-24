#include "world_manager.hpp"
#include "game/camera.hpp"
#include "game/game.hpp"
#include "structures.hpp"
#include "util/thread_pool.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"
#include <chrono>
#include <cstdio>
#include <future>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <unordered_map>
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
                        util::map(dist(gen), 0.0f, 1.0f, 0.0f, 184.0f),
                        util::map(dist(gen), 0.0f, 1.0f, -384.0f, 384.0f) + 32.0f,
                        24.0f}},
                    .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), 96}}}));
        }
    }

    WorldManager::~WorldManager()
    {
        this->chunks.clear();

        for (voxel::ChunkRenderManager::RaytracedLight& l : this->raytraced_lights)
        {
            this->chunk_render_manager.destroyRaytracedLight(std::move(l));
        }

        for (VoxelObject& o : this->voxel_object_deletion_queue)
        {
            this->voxel_object_allocator.free(std::move(o));
        }
    }

    WorldManager::VoxelObject
    WorldManager::createVoxelObject(LinearVoxelVolume volume, voxel::WorldPosition position)
    {
        VoxelObject newObject = this->voxel_object_allocator.allocateOrPanic();

        this->voxel_object_tracking_data[this->voxel_object_allocator.getValueOfHandle(newObject)] =
            VoxelObjectTrackingData {
                .volume {std::move(volume)}, .next_position {position}, .current_position {}};

        return newObject;
    }

    void
    WorldManager::setVoxelObjectPosition(const VoxelObject& voxelObject, WorldPosition newPosition)
    {
        this->voxel_object_tracking_data[this->voxel_object_allocator.getValueOfHandle(voxelObject)]
            .next_position = newPosition;
    }

    void WorldManager::destroyVoxelObject(VoxelObject voxelObject)
    {
        util::assertFatal(!voxelObject.isNull(), "Tried to destroy null Voxel Object!");

        this->voxel_object_tracking_data[this->voxel_object_allocator.getValueOfHandle(voxelObject)]
            .should_be_deleted = true;

        this->voxel_object_deletion_queue.push_back(std::move(voxelObject));
    }

    std::vector<game::FrameGenerator::RecordObject> WorldManager::onFrameUpdate(
        const game::Camera& camera, gfx::profiler::TaskGenerator& profilerTaskGenerator)
    {
        const i32 radius = 12;
        const auto [cameraChunkBase, _] =
            splitWorldPosition(WorldPosition {static_cast<glm::i32vec3>(camera.getPosition())});
        const glm::i32vec3 chunkRangeBase = cameraChunkBase - radius;
        const glm::i32vec3 chunkRangePeak = cameraChunkBase + radius;

        const int maxSpawns     = 8;
        int       currentSpawns = 0;

        for (int x = chunkRangeBase.x; x <= chunkRangePeak.x; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                for (int z = chunkRangeBase.z; z <= chunkRangePeak.z; ++z)
                {
                    const voxel::WorldPosition pos {{(64 * x), (64 * y), (64 * z)}};

                    const decltype(this->chunks)::const_iterator maybeIt = this->chunks.find(pos);

                    if (maybeIt == this->chunks.cend())
                    {
                        this->chunks.insert(
                            {pos,
                             LazilyGeneratedChunk {
                                 &this->chunk_render_manager, &this->world_generator, pos}});

                        if (currentSpawns++ > maxSpawns)
                        {
                            goto early_exit;
                        }
                    }
                }
            }
        }
    early_exit:

        profilerTaskGenerator.stamp("iterate and generate");

        // Erase chunks that are too far
        std::erase_if(
            this->chunks,
            [&](decltype(this->chunks)::value_type& item)
            {
                auto& [pos, chunk] = item;

                return !(
                    glm::all(glm::lessThanEqual(pos, chunkRangePeak * 64))
                    && glm::all(glm::greaterThanEqual(pos, chunkRangeBase * 64)));
            });

        profilerTaskGenerator.stamp("erase far");

        struct WorldChange
        {
            Voxel         new_voxel;
            WorldPosition new_position;
            bool          should_be_visible;
        };

        std::vector<WorldChange> changes;

        // Handle VoxelObjects
        this->voxel_object_allocator.iterateThroughAllocatedElements(
            [&](const u32 voxelObjectId)
            {
                VoxelObjectTrackingData& thisData = this->voxel_object_tracking_data[voxelObjectId];

                auto emitClearing = [&]
                {
                    thisData.volume.temp_data.iterateOverVoxels(
                        [&](const BrickLocalPosition localPos, const Voxel v)
                        {
                            if (v != Voxel::NullAirEmpty)
                            {
                                changes.push_back(WorldChange {
                                    .new_voxel {Voxel::NullAirEmpty},
                                    .new_position {
                                        static_cast<glm::i32vec3>(localPos)
                                        + *thisData.current_position},
                                    .should_be_visible {false}});
                            }
                        });
                };

                auto emitVolume = [&]
                {
                    thisData.volume.temp_data.iterateOverVoxels(
                        [&](const BrickLocalPosition localPos, const Voxel v)
                        {
                            if (v != Voxel::NullAirEmpty)
                            {
                                changes.push_back(WorldChange {
                                    .new_voxel {v},
                                    .new_position {
                                        static_cast<glm::i32vec3>(localPos)
                                        + thisData.next_position},
                                    .should_be_visible {true}});
                            }
                        });
                };

                if (thisData.should_be_deleted)
                {
                    emitClearing();
                }
                else if (thisData.current_position != thisData.next_position)
                {
                    if (thisData.current_position.has_value())
                    {
                        emitClearing();
                    }

                    emitVolume();
                }

                thisData.current_position = thisData.next_position;
            });

        profilerTaskGenerator.stamp("Voxel Object change collection");

        for (VoxelObject& o : this->voxel_object_deletion_queue)
        {
            this->voxel_object_allocator.free(std::move(o));
        }
        this->voxel_object_deletion_queue.clear();

        std::unordered_map<const LazilyGeneratedChunk*, std::vector<voxel::ChunkLocalUpdate>>
            voxelObjectUpdates {};

        for (WorldChange w : changes)
        {
            const auto [chunkBase, chunkLocal] = splitWorldPosition(w.new_position);
            const decltype(this->chunks)::const_iterator maybeChunkIt =
                this->chunks.find(voxel::WorldPosition {chunkBase * 64});

            if (maybeChunkIt != this->chunks.cend())
            {
                voxelObjectUpdates[&maybeChunkIt->second].push_back(ChunkLocalUpdate {
                    chunkLocal,
                    w.new_voxel,
                    ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
                    w.should_be_visible ? ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible
                                        : ChunkLocalUpdate::CameraVisibleUpdate::CameraInvisible});
            }
        }

        profilerTaskGenerator.stamp("organize VoxelObject changes");

        std::size_t updatesOcurred = 0;

        for (auto& [_, lazyChunk] : this->chunks)
        {
            std::span<const voxel::ChunkLocalUpdate> maybeUpdates {};

            if (const decltype(voxelObjectUpdates)::const_iterator maybeVoxelObjectUpdatesIt =
                    voxelObjectUpdates.find(&lazyChunk);
                maybeVoxelObjectUpdatesIt != voxelObjectUpdates.cend())
            {
                maybeUpdates = std::span {maybeVoxelObjectUpdatesIt->second};
            }

            lazyChunk.updateAndFlushUpdates(maybeUpdates, updatesOcurred);
        }

        profilerTaskGenerator.stamp("dispatch VoxelObject changes");

        std::vector<game::FrameGenerator::RecordObject> recordObjects =
            this->chunk_render_manager.processUpdatesAndGetDrawObjects(
                camera, profilerTaskGenerator);

        return recordObjects;
    }

    WorldManager::LazilyGeneratedChunk::LazilyGeneratedChunk(
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator,
        voxel::WorldPosition   rootPosition)
        : chunk_render_manager {chunkRenderManager}
        , world_generator {worldGenerator}
        , chunk {this->chunk_render_manager->createChunk(rootPosition)}
        , updates {util::runAsync(
              [wg = worldGenerator, pos = rootPosition]
              {
                  return wg->generateChunk(pos);
              })}
    {}

    WorldManager::LazilyGeneratedChunk::~LazilyGeneratedChunk()
    {
        if (this->updates.valid())
        {
            this->updates.wait();
        }

        if (!this->chunk.isNull())
        {
            this->chunk_render_manager->destroyChunk(std::move(this->chunk));
        }
    }

    void WorldManager::LazilyGeneratedChunk::updateAndFlushUpdates(
        std::span<const voxel::ChunkLocalUpdate> extraUpdates, std::size_t& updatesOcurred)
    {
        if (updatesOcurred < 3 && this->updates.valid()
            && this->updates.wait_for(std::chrono::years {0}) == std::future_status::ready)
        {
            updatesOcurred += 1;

            std::vector<voxel::ChunkLocalUpdate> realUpdates = this->updates.get();

            if (!realUpdates.empty())
            {
                this->chunk_render_manager->updateChunk(this->chunk, realUpdates);
            }
        }

        if (!extraUpdates.empty() && extraUpdates.data() != nullptr)
        {
            this->chunk_render_manager->updateChunk(this->chunk, extraUpdates);
        }
    }
} // namespace voxel