#pragma once

#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "lazily_generated_chunk.hpp"
#include "shaders/include/common.glsl"
#include "util/thread_pool.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/structures.hpp"
#include "world/generator.hpp"
#include <glm/geometric.hpp>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace voxel
{

    class VoxelChunkOctree
    {
    public:
        explicit VoxelChunkOctree(
            util::ThreadPool&, ChunkRenderManager*, world::WorldGenerator*, u32 dimension);
        ~VoxelChunkOctree() = default;

        VoxelChunkOctree(const VoxelChunkOctree&)             = delete;
        VoxelChunkOctree(VoxelChunkOctree&&)                  = delete;
        VoxelChunkOctree& operator= (const VoxelChunkOctree&) = delete;
        VoxelChunkOctree& operator= (VoxelChunkOctree&&)      = delete;

        void
        update(const game::Camera&, util::ThreadPool&, ChunkRenderManager*, world::WorldGenerator*);

    private:
        enum class OctreeNodePosition : std::uint8_t
        {
            NxNyNz = 0b000,
            NxNyPz = 0b001,
            NxPyNz = 0b010,
            NxPyPz = 0b011,
            PxNyNz = 0b100,
            PxNyPz = 0b101,
            PxPyNz = 0b110,
            PxPyPz = 0b111,
        };

        static constexpr std::array<glm::ivec3, 8>
        generateChildrenRootPositions(voxel::ChunkLocation location)
        {
            const i32 halfChunkDimension =
                static_cast<i32>(gpu_calculateChunkWidthUnits(location.lod) / 2);
            std::array<glm::ivec3, 8> output {};

            for (std::size_t i = 0; i < 8; ++i)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                output[i] = glm::ivec3 {
                    location.root_position.x + (((i & 0b100u) != 0) ? halfChunkDimension : 0),
                    location.root_position.y + (((i & 0b010u) != 0) ? halfChunkDimension : 0),
                    location.root_position.z + (((i & 0b001u) != 0) ? halfChunkDimension : 0),
                };
            }

            return output;
        }

        struct Node
        {
            Node(
                voxel::ChunkLocation   bounds,
                util::ThreadPool&      threadPool,
                ChunkRenderManager*    chunkRenderManager,
                world::WorldGenerator* worldGenerator)
                : entire_bounds {bounds}
                , payload {
                      std::in_place_index<0>,
                      threadPool,
                      chunkRenderManager,
                      worldGenerator,
                      bounds}
            {}
            ~Node()
            {
                LazilyGeneratedChunk* const chunk = std::get_if<0>(&this->payload);

                if (chunk != nullptr)
                {
                    chunk->markShouldNotGenerate();
                }
            }
            voxel::ChunkLocation                                                     entire_bounds;
            std::variant<LazilyGeneratedChunk, std::array<std::unique_ptr<Node>, 8>> payload;

            void update(
                const game::Camera&,
                util::ThreadPool&,
                ChunkRenderManager*,
                world::WorldGenerator*);

            // void update(const world::WorldGenerator& worldGenerator, glm::vec3 cameraPosition)
            // {
            //     const u32 desiredLOD =
            //     calculateLODBasedOnDistance(static_cast<f32>(glm::distance(
            //         static_cast<glm::f64vec3>(cameraPosition),
            //         static_cast<glm::f64vec3>(this->entire_bounds.getCenterPosition()))));

            //     switch (this->payload.index())
            //     {
            //     case 0: {
            //         LazilyGeneratedChunk* const chunk =
            //             std::get_if<LazilyGeneratedChunk>(&this->payload);

            //         if (desiredLOD == this->entire_bounds.lod)
            //         { /* do nothing */
            //         }
            //         else if (desiredLOD > this->entire_bounds.lod)
            //         { // we shouldn't exist
            //             util::logWarn("we shouldn't exist!");
            //         }
            //         else
            //         { // Subdivide
            //             chunk->leak();

            //             this->payload.emplace(this->generateChildren(worldGenerator));
            //             // we're too far, we shouldn;t exist
            //         }
            //         break;
            //     }
            //     case 1: {
            //         std::array<std::unique_ptr<Node>, 8>* const nodes =
            //             std::get_if<std::array<std::unique_ptr<Node>, 8>>(&this->payload);

            //         if (desiredLOD == this->entire_bounds.lod)
            //         {
            //             this->generateSingle(worldGenerator);
            //         }
            //         else if (desiredLOD > this->entire_bounds.lod)
            //         { // we shouldn't exist
            //             util::logWarn("we shouldn't exist!");
            //         }
            //         else
            //         { // Subdivide
            //             chunk->leak();

            //             this->payload.emplace(this->generateChildren());
            //             // we're too far, we shouldn;t exist
            //         }
            //         break;
            //     }
            //     default:
            //         std::unreachable();
            //     }
            // }
        };

        Node root;
    };

    class LodWorldManager
    {
    public:
        explicit LodWorldManager(const game::Game*, u32 dimension = 65536);
        ~LodWorldManager();

        LodWorldManager(const LodWorldManager&)             = delete;
        LodWorldManager(LodWorldManager&&)                  = delete;
        LodWorldManager& operator= (const LodWorldManager&) = delete;
        LodWorldManager& operator= (LodWorldManager&&)      = delete;

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
        onFrameUpdate(const game::Camera&, gfx::profiler::TaskGenerator&);
    private:
        std::vector<voxel::ChunkRenderManager::RaytracedLight> temporary_raytraced_lights;

        util::ThreadPool      chunk_generation_thread_pool;
        ChunkRenderManager    chunk_render_manager;
        world::WorldGenerator generator;

        std::unique_ptr<VoxelChunkOctree> tree;
    };
} // namespace voxel