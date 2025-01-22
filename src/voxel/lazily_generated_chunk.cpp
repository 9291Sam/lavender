

#include "lazily_generated_chunk.hpp"

namespace voxel
{

    LazilyGeneratedChunk::LazilyGeneratedChunk(
        util::ThreadPool&      pool,
        ChunkRenderManager*    chunkRenderManager,
        world::WorldGenerator* worldGenerator,
        voxel::ChunkLocation   location)
        : chunk_render_manager {chunkRenderManager}
        , chunk {this->chunk_render_manager->createChunk(location)}
        , should_still_generate(std::make_shared<std::atomic<bool>>(true))
        , updates {pool.executeOnPool(
              [wg  = worldGenerator,
               loc = location,
               shouldGeneratePtr =
                   this->should_still_generate]() -> std::vector<voxel::ChunkLocalUpdate>
              {
                  if (shouldGeneratePtr->load(std::memory_order_acquire))
                  {
                      util::logTrace("async generation of chunk");
                      return wg->generateChunk(loc);
                  }
                  else
                  {
                      return {};
                  }
              })}
    {}

    LazilyGeneratedChunk::~LazilyGeneratedChunk()
    {
        if (this->should_still_generate != nullptr)
        {
            this->should_still_generate->store(false, std::memory_order_release);
        }

        if (this->updates.valid())
        {
            this->updates.wait();
        }

        if (!this->chunk.isNull())
        {
            this->chunk_render_manager->destroyChunk(std::move(this->chunk));
        }
    }

    void LazilyGeneratedChunk::updateAndFlushUpdates(
        std::span<const voxel::ChunkLocalUpdate> extraUpdates, std::size_t& updatesOcurred)
    {
        if (this->updates.valid()
            && this->updates.wait_for(std::chrono::years {0}) == std::future_status::ready)
        {
            updatesOcurred += 1;

            std::vector<voxel::ChunkLocalUpdate> realUpdates = this->updates.get();

            if (!realUpdates.empty())
            {
                this->chunk_render_manager->updateChunk(this->chunk, realUpdates);
            }
        }

        if (!extraUpdates.empty() && extraUpdates.data() != nullptr)
        {
            this->chunk_render_manager->updateChunk(this->chunk, extraUpdates);
        }
    }
} // namespace voxel