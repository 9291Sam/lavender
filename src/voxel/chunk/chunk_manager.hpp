#pragma once

#include "chunk.hpp"
#include "game/frame_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/tracked_buffer.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "voxel/brick/brick_map.hpp"
#include "voxel/brick/brick_pointer_allocator.hpp"
#include "voxel/data/greedy_voxel_face.hpp"
#include "voxel/data/material_brick.hpp"
#include "voxel/voxel.hpp"

namespace voxel::chunk
{
    struct ChunkLocalPosition : glm::u8vec3
    {};

    struct InternalChunkData
    {
        glm::vec4                                           position;
        std::optional<std::array<util::RangeAllocation, 6>> face_data;
        bool                                                needs_remesh;
    };

    class ChunkManager
    {
    public:
        explicit ChunkManager(const gfx::Renderer*);
        ~ChunkManager();

        ChunkManager(const ChunkManager&)             = delete;
        ChunkManager(ChunkManager&&)                  = delete;
        ChunkManager& operator= (const ChunkManager&) = delete;
        ChunkManager& operator= (ChunkManager&&)      = delete;

        [[nodiscard]] game::FrameGenerator::RecordObject makeRecordObject();

        Chunk allocateChunk(glm::vec3 position);
        void  deallocateChunk(Chunk&&);

        void writeVoxelToChunk(Chunk, ChunkLocalPosition, Voxel);

    private:
        std::array<util::RangeAllocation, 6> remeshChunk(u32 chunkId);

        const gfx::Renderer* renderer;

        util::IndexAllocator                        chunk_id_allocator;
        std::vector<InternalChunkData>              chunk_data;
        gfx::vulkan::TrackedBuffer<brick::BrickMap> brick_maps;

        struct ChunkDrawIndirectInstancePayload
        {
            glm::vec4 position;
            u32       normal;
        };
        gfx::vulkan::Buffer<ChunkDrawIndirectInstancePayload> indirect_payload;

        brick::BrickPointerAllocator                    brick_allocator;
        gfx::vulkan::TrackedBuffer<data::MaterialBrick> material_bricks;

        util::RangeAllocator                       voxel_face_allocator;
        gfx::vulkan::Buffer<data::GreedyVoxelFace> voxel_faces;
    };
} // namespace voxel::chunk
