#include "world_manager.hpp"
#include "game/camera.hpp"
#include "game/game.hpp"
#include "structures.hpp"
#include "util/thread_pool.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace voxel
{
    static constexpr std::size_t MaxVoxelObjects = 4096;

    WorldManager::WorldManager(const game::Game* game)
        : generation_threads {3}
        , chunk_render_manager {game}
        , world_generator {334734734}
        , voxel_object_allocator {MaxVoxelObjects}
    {
        this->voxel_object_tracking_data.resize(MaxVoxelObjects, VoxelObjectTrackingData {});

        std::mt19937_64                     gen {7384375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};

        for (int i = 0; i < 128; ++i)
        {
            this->raytraced_lights.push_back(
                this->chunk_render_manager.createRaytracedLight(voxel::GpuRaytracedLight {
                    .position_and_half_intensity_distance {glm::vec4 {
                        util::map(dist(gen), 0.0f, 1.0f, -384.0f, 320.0f),
                        util::map(dist(gen), 0.0f, 1.0f, 0.0f, 64.0f),
                        util::map(dist(gen), 0.0f, 1.0f, -384.0f, 320.0f),
                        8.0f}},
                    .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), 96}}}));
        }
    }

    WorldManager::~WorldManager()
    {
        for (auto& [pos, chunk] : this->chunks)
        {
            chunk.markShouldNotGenerate();
        }

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

    boost::dynamic_bitset<u64>
    WorldManager::readVoxelOccupied(std::span<const WorldPosition> positions)
    {
        util::assertFatal(
            positions.size() < std::numeric_limits<u32>::max(), "Tried to poll too many positions");

        struct ReadbackInfo
        {
            std::vector<ChunkLocalPosition> chunk_local_positions;
            std::vector<u32>                positions_in_input_buffer;
        };

        std::unordered_map<const ChunkRenderManager::Chunk*, ReadbackInfo> readbackMap {};

        for (const auto [index, p] : std::views::enumerate(positions))
        {
            const auto [coordinate, position] = splitWorldPosition(p);

            const decltype(this->chunks)::const_iterator maybeIt = this->chunks.find(coordinate);

            if (maybeIt != this->chunks.cend())
            {
                ReadbackInfo& r = readbackMap[maybeIt->second.getChunk()];

                r.chunk_local_positions.push_back(position);
                r.positions_in_input_buffer.push_back(static_cast<u32>(index));
            }
        }

        boost::dynamic_bitset<u64> result {};
        result.resize(positions.size());

        for (const auto& [chunk, readbackInfo] : readbackMap)
        {
            const boost::dynamic_bitset<u64> chunkLocalReads =
                this->chunk_render_manager.readShadow(*chunk, readbackInfo.chunk_local_positions);

            util::assertFatal(
                chunkLocalReads.size() == readbackInfo.positions_in_input_buffer.size(),
                "Dont mess this up");

            for (std::size_t i = 0; i < chunkLocalReads.size(); ++i)
            {
                result.set(readbackInfo.positions_in_input_buffer[i], chunkLocalReads[i]);
            }
        }

        return result;
    }

    std::vector<game::FrameGenerator::RecordObject> WorldManager::onFrameUpdate(
        const game::Camera& camera, gfx::profiler::TaskGenerator& profilerTaskGenerator)
    {
        const i32 radius = 6;
        const auto [cameraChunkBase, _] =
            splitWorldPosition(WorldPosition {static_cast<glm::i32vec3>(camera.getPosition())});
        const voxel::ChunkCoordinate chunkRangeBase =
            voxel::ChunkCoordinate {cameraChunkBase - radius};
        const voxel::ChunkCoordinate chunkRangePeak =
            voxel::ChunkCoordinate {cameraChunkBase + radius};

        // const int maxSpawns     = 256;
        // int       currentSpawns = 0;

        for (int y = chunkRangePeak.y; y >= chunkRangeBase.y; --y)
        {
            for (int x = chunkRangeBase.x; x <= chunkRangePeak.x; ++x)
            {
                for (int z = chunkRangeBase.z; z <= chunkRangePeak.z; ++z)
                {
                    const voxel::ChunkCoordinate coordinate {x, y, z};

                    const decltype(this->chunks)::const_iterator maybeIt =
                        this->chunks.find(coordinate);

                    if (maybeIt == this->chunks.cend())
                    {
                        this->chunks.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(coordinate),
                            std::forward_as_tuple(
                                this->generation_threads,
                                &this->chunk_render_manager,
                                &this->world_generator,
                                coordinate));

                        // if (currentSpawns++ > maxSpawns)
                        // {
                        //     goto early_exit;
                        // }
                    }
                }
            }
        }
        // early_exit:

        profilerTaskGenerator.stamp("iterate and generate");

        // Erase chunks that are too far
        std::erase_if(
            this->chunks,
            [&](decltype(this->chunks)::value_type& item)
            {
                auto& [pos, chunk] = item;

                const bool shouldErase =
                    !(glm::all(glm::lessThanEqual(pos, chunkRangePeak))
                      && glm::all(glm::greaterThanEqual(pos, chunkRangeBase)));

                if (shouldErase)
                {
                    chunk.leak();
                }

                return shouldErase;
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
            const auto [chunkCoordinate, chunkLocal] = splitWorldPosition(w.new_position);
            const decltype(this->chunks)::const_iterator maybeChunkIt =
                this->chunks.find(chunkCoordinate);

            if (maybeChunkIt != this->chunks.cend())
            {
                voxelObjectUpdates[&maybeChunkIt->second].push_back(ChunkLocalUpdate {
                    chunkLocal,
                    w.new_voxel,
                    w.should_be_visible ? ChunkLocalUpdate::ShadowUpdate::ShadowCasting
                                        : ChunkLocalUpdate::ShadowUpdate::ShadowTransparent,
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
        util::ThreadPool&      pool,
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator,
        voxel::ChunkCoordinate coordinate)
        : chunk_render_manager {chunkRenderManager}
        , world_generator {worldGenerator}
        , chunk {this->chunk_render_manager->createChunk(coordinate)}
        , should_still_generate(std::make_shared<std::atomic<bool>>(true))
        , updates {pool.executeOnPool(
              [wg    = worldGenerator,
               coord = coordinate,
               shouldGeneratePtr =
                   this->should_still_generate]() -> std::vector<voxel::ChunkLocalUpdate>
              {
                  if (shouldGeneratePtr->load(std::memory_order_acquire))
                  {
                      return wg->generateChunk(coord);
                  }
                  else
                  {
                      return {};
                  }
              })}
    {}

    WorldManager::LazilyGeneratedChunk::~LazilyGeneratedChunk()
    {
        if (this->should_still_generate != nullptr)
        {
            this->should_still_generate->store(false, std::memory_order_release);
        }

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
        if (this->updates.valid()
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