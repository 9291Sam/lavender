#pragma once

#include "chunk_render_manager.hpp"
#include "game/frame_generator.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "structures.hpp"
#include "util/opaque_integer_handle.hpp"
#include "util/thread_pool.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "world/generator.hpp"

namespace voxel
{
    struct LinearVoxelVolume
    {
        MaterialBrick temp_data;
    };

    class WorldManager
    {
    public:
        using VoxelObject = util::OpaqueHandle<"voxel::WorldManager::VoxelObject", u32>;
    public:
        explicit WorldManager(const game::Game*);
        ~WorldManager();

        WorldManager(const WorldManager&)             = delete;
        WorldManager(WorldManager&&)                  = delete;
        WorldManager& operator= (const WorldManager&) = delete;
        WorldManager& operator= (WorldManager&&)      = delete;

        VoxelObject createVoxelObject(LinearVoxelVolume, WorldPosition);
        void        setVoxelObjectPosition(const VoxelObject&, WorldPosition);
        void        destroyVoxelObject(VoxelObject);

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
        onFrameUpdate(const game::Camera&, gfx::profiler::TaskGenerator&);

    private:
        class LazilyGeneratedChunk
        {
        public:
            explicit LazilyGeneratedChunk(
                util::ThreadPool&,
                ChunkRenderManager*,
                world::WorldGenerator*,
                voxel::WorldPosition rootPosition);
            ~LazilyGeneratedChunk();

            LazilyGeneratedChunk(const LazilyGeneratedChunk&)             = delete;
            LazilyGeneratedChunk(LazilyGeneratedChunk&&)                  = default;
            LazilyGeneratedChunk& operator= (const LazilyGeneratedChunk&) = delete;
            LazilyGeneratedChunk& operator= (LazilyGeneratedChunk&&)      = default;

            void wait()
            {
                this->updates.wait();
            }

            void updateAndFlushUpdates(
                std::span<const voxel::ChunkLocalUpdate> extraUpdates, std::size_t& updatesOcurred);

        private:
            ChunkRenderManager*       chunk_render_manager;
            world::WorldGenerator*    world_generator;
            ChunkRenderManager::Chunk chunk;

            std::future<std::vector<voxel::ChunkLocalUpdate>> updates;
        };

        util::ThreadPool      generation_threads;
        ChunkRenderManager    chunk_render_manager;
        world::WorldGenerator world_generator;

        struct VoxelObjectTrackingData
        {
            LinearVoxelVolume                   volume {};
            voxel::WorldPosition                next_position {{0, 0, 0}};
            std::optional<voxel::WorldPosition> current_position;
            bool                                should_be_deleted = false;
        };

        util::OpaqueHandleAllocator<VoxelObject> voxel_object_allocator;
        std::vector<VoxelObjectTrackingData>     voxel_object_tracking_data;
        std::vector<VoxelObject>                 voxel_object_deletion_queue;

        std::unordered_map<voxel::WorldPosition, LazilyGeneratedChunk> chunks;

        std::vector<ChunkRenderManager::RaytracedLight> raytraced_lights;
    };
} // namespace voxel