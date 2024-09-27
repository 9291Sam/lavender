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
#include "util/range_allocator.hpp"
#include "voxel.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/visibility_brick.hpp"
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

            // returns false on out of bounds access
            [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
            {
                if (p.x < 0 || p.x >= 64 || p.y < 0 || p.y >= 64 || p.z < 0
                    || p.z >= 64)
                {
                    return false;
                }
                else
                {
                    return static_cast<bool>(
                        this->data[p.x][p.y] & (1ULL << static_cast<u64>(p.z)));
                }
            }
            void write(glm::i8vec3, bool);
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
