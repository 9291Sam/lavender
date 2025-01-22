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
#include <memory>
#include <random>

namespace voxel
{
    VoxelChunkOctree::VoxelChunkOctree(
        util::ThreadPool&      threadPool,
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator,
        u32                    dimension)
        : root {
              voxel::ChunkLocation {Gpu_ChunkLocation {
                  .root_position {glm::ivec3 {
                      -static_cast<i32>(dimension / 2),
                      -static_cast<i32>(dimension / 2),
                      -static_cast<i32>(dimension / 2),
                  }},
                  .lod {calculateLODBasedOnDistance<u32>(dimension)},
              }},
              threadPool,
              chunkRenderManager,
              worldGenerator}

    {}

    void VoxelChunkOctree::update(
        const game::Camera&    camera,
        util::ThreadPool&      threadPool,
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator)
    {
        this->root.update(camera, threadPool, chunkRenderManager, worldGenerator);
    }

    void VoxelChunkOctree::Node::update(
        const game::Camera&    camera,
        util::ThreadPool&      threadPool,
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator)
    {
        const u32 desiredLOD = calculateLODBasedOnDistance(static_cast<f32>(glm::distance(
            static_cast<glm::f64vec3>(camera.getPosition()),
            static_cast<glm::f64vec3>(this->entire_bounds.getCenterPosition()))));

        if (this->payload.index() == 0)
        {
            LazilyGeneratedChunk* const chunk = std::get_if<0>(&this->payload);

            if (this->entire_bounds.lod > desiredLOD)
            {
                chunk->markShouldNotGenerate();

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

            if (this->entire_bounds.lod < desiredLOD)
            {
                *children = {};

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
    }

    LodWorldManager::LodWorldManager(const game::Game* game, u32 dimension)
        : chunk_generation_thread_pool {4}
        , chunk_render_manager {game}
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

        for (int i = 0; i < 32; ++i)
        {
            this->temporary_raytraced_lights.push_back(
                this->chunk_render_manager.createRaytracedLight(voxel::GpuRaytracedLight {
                    .position_and_half_intensity_distance {glm::vec4 {
                        util::map(dist(gen), 0.0f, 1.0f, -64.0f, 128.0f),
                        util::map(dist(gen), 0.0f, 1.0f, 0.0f, 64.0f),
                        util::map(dist(gen), 0.0f, 1.0f, -64.0f, 128.0f),
                        12.0f}},
                    .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), 256}}}));
        }
    }

    LodWorldManager::~LodWorldManager()
    {
        util::logTrace("Destroying world octree");
        this->tree.reset();

        util::logTrace("Destroying everything else");

        for (voxel::ChunkRenderManager::RaytracedLight& l : this->temporary_raytraced_lights)
        {
            this->chunk_render_manager.destroyRaytracedLight(std::move(l));
        }
    }

    std::vector<game::FrameGenerator::RecordObject> LodWorldManager::onFrameUpdate(
        const game::Camera& camera, gfx::profiler::TaskGenerator& taskGenerator)
    {
        this->tree->update(
            camera,
            this->chunk_generation_thread_pool,
            &this->chunk_render_manager,
            &this->generator);

        return this->chunk_render_manager.processUpdatesAndGetDrawObjects(camera, taskGenerator);
    }

} // namespace voxel