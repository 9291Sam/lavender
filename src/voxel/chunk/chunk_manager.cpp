

#include "chunk_manager.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
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

} // namespace voxel::chunk
