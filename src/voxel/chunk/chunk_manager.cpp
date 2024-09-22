

#include "chunk_manager.hpp"
#include "chunk.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "voxel/brick/brick_map.hpp"
#include "voxel/brick/brick_pointer.hpp"
#include "voxel/brick/brick_pointer_allocator.hpp"
#include <cstddef>
#include <vulkan/vulkan_enums.hpp>

namespace voxel::chunk
{
    static constexpr u32 MaxChunks = 4096;

    ChunkManager::ChunkManager(const gfx::Renderer* renderer_)
        : renderer {renderer_}
        , chunk_id_allocator {MaxChunks}
        , chunk_data {MaxChunks, InternalChunkData {}}
        , brick_maps(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              MaxChunks)
        , indirect_payload(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * 6))
        , brick_allocator(MaxChunks * 8 * 8 * 2)
        , material_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * 8 * 8 * 4))
        , voxel_face_allocator {16 * 1024 * 1024, MaxChunks * 6}
        , voxel_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(16 * 1024 * 1024))
    {
        util::logTrace(
            "allocated {} bytes", gfx::vulkan::bufferBytesAllocated.load());
    }

    ChunkManager::~ChunkManager() = default;

    Chunk ChunkManager::allocateChunk(glm::vec3 position)
    {
        const std::expected<u32, util::IndexAllocator::OutOfBlocks>
            maybeThisChunkId = this->chunk_id_allocator.allocate();

        if (!maybeThisChunkId.has_value())
        {
            util::panic("Failed to allocate new chunk!");
        }

        const u32 thisChunkId = *maybeThisChunkId;

        this->chunk_data[thisChunkId] = InternalChunkData {
            .position {glm::vec4 {position.x, position.y, position.z, 0.0}},
            .face_data {std::nullopt},
            .needs_remesh {true}};

        const brick::BrickMap emptyBrickMap {};
        this->brick_maps.write(thisChunkId, {&emptyBrickMap, 1});

        return Chunk {thisChunkId};
    }

    void ChunkManager::deallocateChunk(Chunk&& chunkToMoveFrom)
    {
        Chunk toFree {std::move(chunkToMoveFrom)};

        this->chunk_id_allocator.free(toFree.id);

        // TODO: iterate over brick maps and free bricks
        brick::BrickMap& thisChunkBrickMap = this->brick_maps.modify(toFree.id);

        for (auto& yz : thisChunkBrickMap.data)
        {
            for (auto& z : yz)
            {
                for (brick::MaybeBrickPointer& ptr : z)
                {
                    if (!ptr.isNull())
                    {
                        this->brick_allocator.free(
                            brick::BrickPointer {ptr.pointer});
                    }
                }
            }
        }

        std::optional<std::array<util::RangeAllocation, 6>>& maybeFacesToFree =
            this->chunk_data[toFree.id].face_data;

        if (maybeFacesToFree.has_value())
        {
            for (util::RangeAllocation allocation : *maybeFacesToFree)
            {
                this->voxel_face_allocator.free(allocation);
            }
        }
    }

    void ChunkManager::writeVoxelToChunk(Chunk c, ChunkLocalPosition p, Voxel v)
    {}

} // namespace voxel::chunk
