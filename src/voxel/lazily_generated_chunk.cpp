

#include "lazily_generated_chunk.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <memory>

namespace voxel
{

    LazilyGeneratedChunk::LazilyGeneratedChunk(
        util::ThreadPool&                pool,
        util::Mutex<ChunkRenderManager>* chunkRenderManager,
        world::WorldGenerator*           worldGenerator,
        voxel::ChunkLocation             location)
        : chunk_render_manager {chunkRenderManager}
        , chunk {this->chunk_render_manager->lock(
              [&](ChunkRenderManager& manager)
              {
                  return manager.createChunk(location);
              })}
        , should_still_generate(std::make_shared<std::atomic<bool>>(true))
        , updates {pool.executeOnPool(
              [wg  = worldGenerator,
               loc = location,
               shouldGeneratePtr =
                   this->should_still_generate]() -> std::vector<voxel::ChunkLocalUpdate>
              {
                  if (shouldGeneratePtr->load(std::memory_order_acquire))
                  {
                      return wg->generateChunk(loc);
                  }
                  else
                  {
                      return {};
                  }
              })}
        , is_meshing_complete {std::make_shared<std::atomic_bool>(false)}
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
            this->chunk_render_manager->lock(
                [&](ChunkRenderManager& manager)
                {
                    manager.destroyChunk(std::move(this->chunk));
                });
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
                this->is_meshing_complete = this->chunk_render_manager->lock(
                    [&](ChunkRenderManager& manager)
                    {
                        return manager.updateChunk(this->chunk, realUpdates);
                    });
            }
            else
            {
                this->is_meshing_complete = std::make_shared<std::atomic_bool>(true);
            }
        }

        if (!extraUpdates.empty() && extraUpdates.data() != nullptr)
        {
            this->chunk_render_manager->lock(
                [&](ChunkRenderManager& manager)
                {
                    manager.updateChunk(this->chunk, extraUpdates);
                });
        }
    }

    bool LazilyGeneratedChunk::isFullyLoaded() const
    {
        if (this->is_meshing_complete != nullptr)
        {
            return this->is_meshing_complete->load();
        }
        else
        {
            return false;
        }
    }

} // namespace voxel