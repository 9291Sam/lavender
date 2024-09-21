#pragma once

#include "game/frame_generator.hpp"
#include "util/index_allocator.hpp"
#include "voxel/voxel.hpp"

namespace voxel::chunk
{
    struct ChunkLocalPosition : glm::u8vec3
    {};

    class Chunk
    {
    public:
        static constexpr U32 NullChunk = static_cast<U32>(-1);
    public:
        Chunk();
        ~Chunk();

        Chunk(const Chunk&)             = delete;
        Chunk(Chunk&&)                  = default;
        Chunk& operator= (const Chunk&) = delete;
        Chunk& operator= (Chunk&&)      = default;


    private:
        explicit Chunk(U32);
        friend class ChunkRenderer;

        U32 Id;
    };

    struct InternalChunkData
    {
        glm::vec4 position;
    };

    class ChunkRenderer
    {
    public:
        ChunkRenderer();
        ~ChunkRenderer();

        ChunkRenderer(const ChunkRenderer&)             = delete;
        ChunkRenderer(ChunkRenderer&&)                  = delete;
        ChunkRenderer& operator= (const ChunkRenderer&) = delete;
        ChunkRenderer& operator= (ChunkRenderer&&)      = delete;

        [[nodiscard]] game::FrameGenerator::RecordObject makeRecordObject();

        Chunk allocateChunk(glm::vec3 position);
        void  deallocateChunk(Chunk&); // TODO: this feels shit

        void writeVoxelToChunk(Chunk, ChunkLocalPosition, Voxel);

    private:
        util::IndexAllocator chunk_id_allocator;

        struct ChunkDrawIndirectPayload
        {
            glm::vec4 position;
            U32       normal;
        };
    };
} // namespace voxel::chunk
