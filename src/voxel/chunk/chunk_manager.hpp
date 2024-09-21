#pragma once

#include "game/frame_generator.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "voxel/brick/brick_allocator.hpp"
#include "voxel/data/greedy_voxel_face.hpp"
#include "voxel/data/material_brick.hpp"
#include "voxel/voxel.hpp"

namespace voxel::chunk
{
    struct ChunkLocalPosition : glm::u8vec3
    {};

    class Chunk
    {
    public:
        static constexpr u32 NullChunk = static_cast<u32>(-1);
    public:
        Chunk();
        ~Chunk();

        Chunk(const Chunk&)             = delete;
        Chunk(Chunk&&)                  = default;
        Chunk& operator= (const Chunk&) = delete;
        Chunk& operator= (Chunk&&)      = default;


    private:
        explicit Chunk(u32);
        friend class ChunkManager;

        u32 id;
    };

    struct InternalChunkData
    {
        glm::vec4                                           position;
        std::array<std::optional<util::RangeAllocation>, 6> directions_faces;
    };

    class ChunkManager
    {
    public:
        ChunkManager();
        ~ChunkManager();

        ChunkManager(const ChunkManager&)             = delete;
        ChunkManager(ChunkManager&&)                  = delete;
        ChunkManager& operator= (const ChunkManager&) = delete;
        ChunkManager& operator= (ChunkManager&&)      = delete;

        [[nodiscard]] game::FrameGenerator::RecordObject makeRecordObject();

        Chunk allocateChunk(glm::vec3 position);
        void  deallocateChunk(Chunk&); // TODO: this feels shit

        void writeVoxelToChunk(Chunk, ChunkLocalPosition, Voxel);

    private:
        util::IndexAllocator chunk_id_allocator;

        struct ChunkDrawIndirectPayload
        {
            glm::vec4 position;
            u32       normal;
        };
        gfx::vulkan::Buffer<ChunkDrawIndirectPayload> indirect_payload;

        brick::BrickPointerAllocator             brick_allocator;
        gfx::vulkan::Buffer<data::MaterialBrick> material_bricks;

        util::RangeAllocator                       voxel_face_allocator;
        gfx::vulkan::Buffer<data::GreedyVoxelFace> voxel_faces;
    };
} // namespace voxel::chunk
