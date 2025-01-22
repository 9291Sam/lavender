#pragma once

#include "util/thread_pool.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"
namespace voxel
{
    class LazilyGeneratedChunk
    {
    public:
        explicit LazilyGeneratedChunk(
            util::ThreadPool&, ChunkRenderManager*, world::WorldGenerator*, voxel::ChunkLocation);
        ~LazilyGeneratedChunk();

        LazilyGeneratedChunk(const LazilyGeneratedChunk&)             = delete;
        LazilyGeneratedChunk(LazilyGeneratedChunk&&)                  = delete;
        LazilyGeneratedChunk& operator= (const LazilyGeneratedChunk&) = delete;
        LazilyGeneratedChunk& operator= (LazilyGeneratedChunk&&)      = delete;

        void markShouldNotGenerate()
        {
            this->should_still_generate->store(false, std::memory_order_release);
        }

        void leak()
        {
            this->should_still_generate->store(false, std::memory_order_release);
            this->updates = {};
        }

        void updateAndFlushUpdates(
            std::span<const voxel::ChunkLocalUpdate> extraUpdates, std::size_t& updatesOcurred);

        [[nodiscard]] const ChunkRenderManager::Chunk* getChunk() const
        {
            return &this->chunk;
        }

    private:
        ChunkRenderManager*                chunk_render_manager;
        ChunkRenderManager::Chunk          chunk;
        std::shared_ptr<std::atomic<bool>> should_still_generate;

        std::future<std::vector<voxel::ChunkLocalUpdate>> updates;
    };
} // namespace voxel