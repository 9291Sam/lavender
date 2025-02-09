#include "lod_world_manager.hpp"
#include "game/camera.hpp"
#include "game/game.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "shaders/include/common.glsl"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/lazily_generated_chunk.hpp"
#include "voxel/structures.hpp"
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <random>

namespace voxel
{
    VoxelChunkOctree::VoxelChunkOctree(
        util::ThreadPool&                threadPool,
        util::Mutex<ChunkRenderManager>* chunkRenderManager,
        world::WorldGenerator*           worldGenerator,
        u32                              dimension)
        : root {
              voxel::ChunkLocation {Gpu_ChunkLocation {
                  .root_position {glm::ivec3 {
                      -static_cast<i32>(dimension / 2),
                      -static_cast<i32>(dimension / 2),
                      -static_cast<i32>(dimension / 2),
                  }},
                  .lod {calculateLODBasedOnDistance(static_cast<f32>(dimension))},
              }},
              threadPool,
              chunkRenderManager,
              worldGenerator}

    {}

    void VoxelChunkOctree::update(
        const game::Camera&              camera,
        util::ThreadPool&                threadPool,
        util::Mutex<ChunkRenderManager>* chunkRenderManager,
        world::WorldGenerator*           worldGenerator)
    {
        this->root.update(camera, threadPool, chunkRenderManager, worldGenerator);
    }

    void VoxelChunkOctree::Node::update(
        const game::Camera&              camera,
        util::ThreadPool&                threadPool,
        util::Mutex<ChunkRenderManager>* chunkRenderManager,
        world::WorldGenerator*           worldGenerator)
    {
        const u32 desiredLOD = calculateLODBasedOnDistance(
            glm::distance(
                camera.getPosition(),
                static_cast<glm::f32vec3>(this->entire_bounds.getCenterPosition()))
            / 2.0f);

        if (this->payload.index() == 0)
        {
            LazilyGeneratedChunk* const chunk = std::get_if<0>(&this->payload);

            if (this->entire_bounds.lod > desiredLOD)
            {
                std::array<std::unique_ptr<Node>, 8> newChildren {};
                std::array<glm::ivec3, 8>            newChildrenRoots =
                    generateChildrenRootPositions(this->entire_bounds);

                for (std::size_t i = 0; i < 8; ++i)
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    newChildren[i] = std::make_unique<Node>(
                        voxel::ChunkLocation {Gpu_ChunkLocation {
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                            .root_position {newChildrenRoots[i]},
                            .lod {this->entire_bounds.lod - 1},
                        }},
                        threadPool,
                        chunkRenderManager,
                        worldGenerator);
                }

                this->previous_payload_lifetime_extension = std::move(this->payload);

                this->payload = std::move(newChildren);
            }
            else
            {
                std::size_t _ {};

                chunk->updateAndFlushUpdates({}, _);
            }
        }
        else
        {
            std::array<std::unique_ptr<Node>, 8>* const children = std::get_if<1>(&this->payload);

            if (this->entire_bounds.lod <= desiredLOD)
            {
                this->previous_payload_lifetime_extension = std::move(this->payload);

                this->payload.emplace<LazilyGeneratedChunk>(
                    threadPool, chunkRenderManager, worldGenerator, this->entire_bounds);
            }
            else
            {
                for (const std::unique_ptr<Node>& c : *children)
                {
                    if (c != nullptr)
                    {
                        c->update(camera, threadPool, chunkRenderManager, worldGenerator);
                    }
                }
            }
        }

        if (this->previous_payload_lifetime_extension.has_value())
        {
            if (this->payload.index() == 0)
            {
                LazilyGeneratedChunk* const chunk = std::get_if<0>(&this->payload);

                std::size_t _ = 0;

                chunk->updateAndFlushUpdates({}, _);
            }
            else
            {
                const std::array<std::unique_ptr<Node>, 8>* const children =
                    std::get_if<1>(&this->payload);

                for (const std::unique_ptr<Node>& p : *children)
                {
                    p->update(camera, threadPool, chunkRenderManager, worldGenerator);
                }
            }

            if (this->isNodeFullyLoaded())
            {
                this->previous_payload_lifetime_extension = std::nullopt;
            }
        }
    }

    bool VoxelChunkOctree::Node::isNodeFullyLoaded() const
    {
        if (this->payload.index() == 0)
        {
            const LazilyGeneratedChunk* const chunk = std::get_if<0>(&this->payload);

            return chunk->isFullyLoaded();
        }
        else
        {
            const std::array<std::unique_ptr<Node>, 8>* const children =
                std::get_if<1>(&this->payload);

            for (const std::unique_ptr<Node>& p : *children)
            {
                if (!p->isNodeFullyLoaded())
                {
                    return false;
                }
            }

            return true;
        }
    }

    LodWorldManager::LodWorldManager(const game::Game* game, u32 dimension)
        : chunk_generation_thread_pool {4}
        , chunk_render_manager {ChunkRenderManager {game}}
        , generator {UINT64_C(879123897234897243)}
        , tree {std::make_unique<VoxelChunkOctree>(
              this->chunk_generation_thread_pool,
              &this->chunk_render_manager,
              &this->generator,
              dimension)}
    {
        std::mt19937_64                     gen {73847375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};
        std::uniform_real_distribution<f32> distN {-1.0f, 1.0f};

        this->chunk_render_manager.lock(
            [&](ChunkRenderManager& manager)
            {
                for (int i = 0; i < 32; ++i)
                {
                    this->temporary_raytraced_lights.push_back(
                        manager.createRaytracedLight(voxel::GpuRaytracedLight {
                            .position_and_half_intensity_distance {glm::vec4 {
                                util::map(dist(gen), 0.0f, 1.0f, -64.0f, 128.0f),
                                util::map(dist(gen), 0.0f, 1.0f, 0.0f, 64.0f),
                                util::map(dist(gen), 0.0f, 1.0f, -64.0f, 128.0f),
                                12.0f}},
                            .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), 256}}}));
                }

                this->temporary_raytraced_lights.push_back(
                    manager.createRaytracedLight(voxel::GpuRaytracedLight {
                        .position_and_half_intensity_distance {
                            glm::vec4 {16384.0f, 16384.0f, 16384.0f, 8192.0f}},
                        .color_and_power {glm::vec4 {1.0f, 1.0f, 1.0f, 256.0f}}}));
            });
    }

    LodWorldManager::~LodWorldManager()
    {
        if (this->maybe_tree_process_future.valid())
        {
            this->maybe_tree_process_future.get();
        }

        util::logTrace("Destroying world octree");
        this->tree.reset();

        util::logTrace("Destroying everything else");

        this->chunk_render_manager.lock(
            [&](ChunkRenderManager& manager)
            {
                for (voxel::ChunkRenderManager::RaytracedLight& l :
                     this->temporary_raytraced_lights)
                {
                    manager.destroyRaytracedLight(std::move(l));
                }
            });
    }

    std::vector<game::FrameGenerator::RecordObject> LodWorldManager::onFrameUpdate(
        const game::Camera& camera, gfx::profiler::TaskGenerator& taskGenerator)
    {
        if (!this->maybe_tree_process_future.valid()
            || this->maybe_tree_process_future.wait_for(std::chrono::years {0})
                   == std::future_status::ready)
        {
            this->maybe_tree_process_future = this->chunk_generation_thread_pool.executeOnPool(
                [this, c = game::Camera {camera}]
                {
                    this->tree->update(
                        c,
                        this->chunk_generation_thread_pool,
                        &this->chunk_render_manager,
                        &this->generator);
                });
        }

        taskGenerator.stamp("update tree");

        return this->chunk_render_manager.lock(
            [&](ChunkRenderManager& manager)
            {
                return manager.processUpdatesAndGetDrawObjects(camera, taskGenerator);
            });
    }

} // namespace voxel