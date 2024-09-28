#pragma once

#include "brick_map.hpp"
#include "brick_pointer_allocator.hpp"
#include "chunk.hpp"
#include "game/frame_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/tracked_buffer.hpp"
#include "greedy_voxel_face.hpp"
#include "material_brick.hpp"
#include "util/index_allocator.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "voxel.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/visibility_brick.hpp"
#include <glm/fwd.hpp>
#include <glm/gtx/string_cast.hpp>
#include <source_location>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    struct InternalChunkData
    {
        glm::vec4                                           position;
        std::optional<std::array<util::RangeAllocation, 6>> face_data;
        bool                                                needs_remesh;
    };

    class ChunkManager
    {
    public:
        explicit ChunkManager(const game::Game*);
        ~ChunkManager();

        ChunkManager(const ChunkManager&)             = delete;
        ChunkManager(ChunkManager&&)                  = delete;
        ChunkManager& operator= (const ChunkManager&) = delete;
        ChunkManager& operator= (ChunkManager&&)      = delete;

        [[nodiscard]] game::FrameGenerator::RecordObject makeRecordObject();

        Chunk allocateChunk(glm::vec3 position);
        void  deallocateChunk(Chunk);

        void writeVoxelToChunk(
            const Chunk&,
            ChunkLocalPosition,
            Voxel,
            std::source_location = std::source_location::current());

    private:
        std::array<util::RangeAllocation, 6> meshChunkNormal(u32 chunkId);
        std::array<util::RangeAllocation, 6> meshChunkGreedy(u32 chunkId);

        struct DenseBitChunk
        {
            std::array<std::array<u64, 64>, 64> data;

            static bool isPositionInBounds(glm::i8vec3 p)
            {
                return p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                    && p.z < 64;
            }

            // returns false on out of bounds access
            [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
            {
                util::assertFatal(
                    p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                        && p.z < 64,
                    "{} {} {}",
                    p.x,
                    p.y,
                    p.z);

                // if (p.x < 0 || p.x >= 64 || p.y < 0 || p.y >= 64 || p.z < 0
                //     || p.z >= 64)
                // {
                //     return false;
                // }
                // else
                // {
                return static_cast<bool>(
                    this->data[p.x][p.y] & (1ULL << static_cast<u64>(p.z)));
                // }
            }
            // if its occupied, it removes it from the data structure
            [[nodiscard]] bool isOccupiedClearing(glm::i8vec3 p)
            {
                const bool result = this->isOccupied(p);

                if (result)
                {
                    this->write(p, false);
                }

                return result;
            }

            [[nodiscard]] bool isEntireRangeOccupied(
                glm::i8vec3 base, glm::i8vec3 step, i8 range) const // NOLINT
            {
                bool isEntireRangeOccupied = true;

                for (i8 i = 0; i < range; ++i)
                {
                    glm::i8vec3 probe = base + step * i;

                    if (!this->isOccupied(probe))
                    {
                        isEntireRangeOccupied = false;
                        break;
                    }
                }

                return isEntireRangeOccupied;
            }

            void clearEntireRange(glm::i8vec3 base, glm::i8vec3 step, i8 range)
            {
                for (i8 i = 0; i < range; ++i)
                {
                    glm::i8vec3 probe = base + step * i;

                    this->write(probe, false);
                }
            }

            void write(glm::i8vec3 p, bool filled)
            {
                // if constexpr (util::isDebugBuild())
                // {
                util::assertFatal(
                    p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                        && p.z < 64,
                    "{} {} {}",
                    p.x,
                    p.y,
                    p.z);
                // }

                if (filled)
                {
                    // NOLINTNEXTLINE
                    this->data[p.x][p.y] |= (1 << static_cast<u64>(p.z));
                }
                else
                {
                    // NOLINTNEXTLINE
                    this->data[p.x][p.y] &= ~(1 << static_cast<u64>(p.z));
                }
            }
        };

        std::unique_ptr<DenseBitChunk> makeDenseBitChunk(u32 chunkId);

        const gfx::Renderer* renderer;

        util::IndexAllocator                 chunk_id_allocator;
        std::vector<InternalChunkData>       chunk_data;
        gfx::vulkan::TrackedBuffer<BrickMap> brick_maps;

        struct ChunkDrawIndirectInstancePayload
        {
            glm::vec4 position;
            u32       normal;
            u32       chunk_id;
        };
        gfx::vulkan::Buffer<ChunkDrawIndirectInstancePayload> indirect_payload;
        gfx::vulkan::Buffer<vk::DrawIndirectCommand>          indirect_commands;

        BrickPointerAllocator                       brick_allocator;
        gfx::vulkan::TrackedBuffer<MaterialBrick>   material_bricks;
        gfx::vulkan::TrackedBuffer<VisibilityBrick> visibility_bricks;

        gfx::vulkan::Buffer<VoxelMaterial> material_buffer;

        util::RangeAllocator                 voxel_face_allocator;
        gfx::vulkan::Buffer<GreedyVoxelFace> voxel_faces;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> descriptor_set_layout;
        std::shared_ptr<vk::UniquePipeline>            chunk_renderer_pipeline;

        vk::DescriptorSet chunk_descriptor_set;
        vk::DescriptorSet global_descriptor_set;
    };
} // namespace voxel
