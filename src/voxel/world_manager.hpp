// #pragma once

// #include "chunk_render_manager.hpp"
// #include "game/frame_generator.hpp"
// #include "gfx/profiler/task_generator.hpp"
// #include "structures.hpp"
// #include "util/opaque_integer_handle.hpp"
// #include "util/thread_pool.hpp"
// #include "voxel/chunk_render_manager.hpp"
// #include "world/generator.hpp"
// #include <atomic>
// #include <boost/dynamic_bitset/dynamic_bitset.hpp>

// namespace voxel
// {
//     struct LinearVoxelVolume
//     {
//         MaterialBrick temp_data;
//     };

//     class WorldManager
//     {
//     public:
//         using VoxelObject = util::OpaqueHandle<"voxel::WorldManager::VoxelObject", u32>;
//     public:
//         explicit WorldManager(const game::Game*);
//         ~WorldManager();

//         WorldManager(const WorldManager&)             = delete;
//         WorldManager(WorldManager&&)                  = delete;
//         WorldManager& operator= (const WorldManager&) = delete;
//         WorldManager& operator= (WorldManager&&)      = delete;

//         VoxelObject createVoxelObject(LinearVoxelVolume, WorldPosition);
//         void        setVoxelObjectPosition(const VoxelObject&, WorldPosition);
//         void        destroyVoxelObject(VoxelObject);

//         boost::dynamic_bitset<u64> readVoxelOccupied(std::span<const voxel::WorldPosition>);

//         [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
//         onFrameUpdate(const game::Camera&, gfx::profiler::TaskGenerator&);

//     private:

//         util::ThreadPool      generation_threads;
//         ChunkRenderManager    chunk_render_manager;
//         world::WorldGenerator world_generator;

//         struct VoxelObjectTrackingData
//         {
//             LinearVoxelVolume                   volume {};
//             voxel::WorldPosition                next_position {{0, 0, 0}};
//             std::optional<voxel::WorldPosition> current_position;
//             bool                                should_be_deleted = false;
//         };

//         util::OpaqueHandleAllocator<VoxelObject> voxel_object_allocator;
//         std::vector<VoxelObjectTrackingData>     voxel_object_tracking_data;
//         std::vector<VoxelObject>                 voxel_object_deletion_queue;

//         std::unordered_map<voxel::ChunkCoordinate, LazilyGeneratedChunk> chunks;

//         std::vector<ChunkRenderManager::RaytracedLight> raytraced_lights;
//     };
// } // namespace voxel