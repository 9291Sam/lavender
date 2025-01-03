// #pragma once

// #include "game/camera.hpp"
// #include "game/frame_generator.hpp"
// #include "gfx/profiler/task_generator.hpp"
// #include "util/thread_pool.hpp"
// #include "voxel/chunk_render_manager.hpp"
// #include "voxel/structures.hpp"
// #include "world/generator.hpp"
// #include <glm/geometric.hpp>
// #include <memory>
// #include <variant>
// #include <vector>

// namespace voxel
// {
//     class LazilyGeneratedChunk
//     {
//     public:
//         explicit LazilyGeneratedChunk(
//             util::ThreadPool&, ChunkRenderManager*, world::WorldGenerator*,
//             voxel::ChunkCoordinate);
//         ~LazilyGeneratedChunk();

//         LazilyGeneratedChunk(const LazilyGeneratedChunk&)             = delete;
//         LazilyGeneratedChunk(LazilyGeneratedChunk&&)                  = delete;
//         LazilyGeneratedChunk& operator= (const LazilyGeneratedChunk&) = delete;
//         LazilyGeneratedChunk& operator= (LazilyGeneratedChunk&&)      = delete;

//         void markShouldNotGenerate()
//         {
//             this->should_still_generate->store(false, std::memory_order_release);
//         }

//         void leak()
//         {
//             this->should_still_generate->store(false, std::memory_order_release);
//             this->updates = {};
//         }

//         void updateAndFlushUpdates(
//             std::span<const voxel::ChunkLocalUpdate> extraUpdates, std::size_t& updatesOcurred);

//         const ChunkRenderManager::Chunk* getChunk() const
//         {
//             return &this->chunk;
//         }

//         [[nodiscard]] glm::i32vec3 getPosition() const;

//     private:
//         ChunkRenderManager*                chunk_render_manager;
//         world::WorldGenerator*             world_generator;
//         ChunkRenderManager::Chunk          chunk;
//         std::shared_ptr<std::atomic<bool>> should_still_generate;

//         std::future<std::vector<voxel::ChunkLocalUpdate>> updates;
//     };

//     class VoxelChunkTree
//     {
//     public:

//         void updateWithCamera(const game::Camera&);

//     private:
//         struct Node
//         {
//             glm::i32vec3 root_position; std::variant<LazilyGeneratedChunk,
//             std::array<std::unique_ptr<Node>, 8>> payload;

//             void subdivide(const world::WorldGenerator& worldGenerator, glm::i32vec3
//             cameraPosition)
//             {
//                 switch (this->payload.index())
//                 {
//                 case 0:
//                     LazilyGeneratedChunk* chunk = std::get_if<0>(&this->payload);
//                 }

//                 this->payload.index() const u32 desiredLOD =
//                     calculateLODBasedOnDistance(static_cast<f32>(glm::distance(
//                         static_cast<glm::f64vec3>(cameraPosition),
//                         static_cast<glm::f64vec3>(this->chunk.getPosition()))));

//                 if (desiredLOD != this->lod)
//                 {
//                     if (desiredLOD < this->lod)
//                     {
//                         destroychildren();
//                         regenerateThis();
//                     }
//                     else
//                     {
//                         destroyThis();
//                         generateChildren();
//                     }
//                 }
//             }
//         };

//         world::WorldGenerator generator;
//         Node                  root;
//     };

//     class LodWorldManager
//     {
//     public:
//         explicit LodWorldManager(u32 lodsToLoad = 7);
//         ~LodWorldManager();

//         LodWorldManager(const LodWorldManager&)             = delete;
//         LodWorldManager(LodWorldManager&&)                  = delete;
//         LodWorldManager& operator= (const LodWorldManager&) = delete;
//         LodWorldManager& operator= (LodWorldManager&&)      = delete;

//         std::vector<game::FrameGenerator::RecordObject>
//         onFrameUpdate(const game::Camera&, gfx::profiler::TaskGenerator&);
//     private:
//         VoxelChunkTree tree;
//     };
// } // namespace voxel