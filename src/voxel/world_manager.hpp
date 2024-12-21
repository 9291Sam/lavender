#pragma once

#include "chunk_render_manager.hpp"
#include "game/frame_generator.hpp"
#include "structures.hpp"
#include "util/opaque_integer_handle.hpp"

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

        // VoxelObject createVoxelObject(LinearVoxelVolume);
        // void        setVoxelObjectPosition(voxel::WorldPosition);
        // void        destroyVoxelObject(VoxelObject);

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
        onFrameUpdate(const game::Camera&);

    private:
        ChunkRenderManager                                                   chunk_render_manager;
        std::future<std::vector<std::unique_ptr<ChunkRenderManager::Chunk>>> chunks;
        util::Mutex<
            std::vector<std::pair<const ChunkRenderManager::Chunk*, std::vector<ChunkLocalUpdate>>>>
                                                        updates;
        std::vector<ChunkRenderManager::RaytracedLight> raytraced_lights;
    };
} // namespace voxel