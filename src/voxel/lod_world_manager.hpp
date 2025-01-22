#pragma once

#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "lazily_generated_chunk.hpp"
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

    class VoxelChunkTree
    {

    public:

        void updateWithCamera(const game::Camera& c)
        {
            this->root.update(this->generator, c.getPosition());
        }

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

        struct Node
        {
            voxel::ChunkLocation                                                     entire_bounds;
            std::variant<LazilyGeneratedChunk, std::array<std::unique_ptr<Node>, 8>> payload;

            void update(const world::WorldGenerator& worldGenerator, glm::vec3 cameraPosition)
            {
                const u32 desiredLOD = calculateLODBasedOnDistance(static_cast<f32>(glm::distance(
                    static_cast<glm::f64vec3>(cameraPosition),
                    static_cast<glm::f64vec3>(this->entire_bounds.getCenterPosition()))));

                switch (this->payload.index())
                {
                case 0: {
                    LazilyGeneratedChunk* const chunk =
                        std::get_if<LazilyGeneratedChunk>(&this->payload);

                    if (desiredLOD == this->entire_bounds.lod)
                    { /* do nothing */
                    }
                    else if (desiredLOD > this->entire_bounds.lod)
                    { // we shouldn't exist
                        util::logWarn("we shouldn't exist!");
                    }
                    else
                    { // Subdivide
                        chunk->leak();

                        this->payload.emplace(this->generateChildren(worldGenerator));
                        // we're too far, we shouldn;t exist
                    }
                    break;
                }
                case 1: {
                    std::array<std::unique_ptr<Node>, 8>* const nodes =
                        std::get_if<std::array<std::unique_ptr<Node>, 8>>(&this->payload);

                    if (desiredLOD == this->entire_bounds.lod)
                    {
                        this->generateSingle(worldGenerator);
                    }
                    else if (desiredLOD > this->entire_bounds.lod)
                    { // we shouldn't exist
                        util::logWarn("we shouldn't exist!");
                    }
                    else
                    { // Subdivide
                        chunk->leak();

                        this->payload.emplace(this->generateChildren());
                        // we're too far, we shouldn;t exist
                    }
                    break;
                }
                default:
                    std::unreachable();
                }
            }
        };

        world::WorldGenerator generator;
        Node                  root;
    };

    class LodWorldManager
    {
    public:
        explicit LodWorldManager(u32 lodsToLoad = 7);
        ~LodWorldManager();

        LodWorldManager(const LodWorldManager&)             = delete;
        LodWorldManager(LodWorldManager&&)                  = delete;
        LodWorldManager& operator= (const LodWorldManager&) = delete;
        LodWorldManager& operator= (LodWorldManager&&)      = delete;

        std::vector<game::FrameGenerator::RecordObject>
        onFrameUpdate(const game::Camera&, gfx::profiler::TaskGenerator&);
    private:
        VoxelChunkTree tree;
    };
} // namespace voxel