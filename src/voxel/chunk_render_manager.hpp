#pragma once

#include "game/frame_generator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "material_manager.hpp"
#include "structures.hpp"
#include "util/index_allocator.hpp"
#include "util/misc.hpp"
#include "util/opaque_integer_handle.hpp"
#include "util/range_allocator.hpp"
#include <source_location>
#include <span>
#include <vulkan/vulkan_handles.hpp>

namespace game
{
    class Game;
} // namespace game

namespace voxel
{
    class ChunkRenderManager
    {
    public:
        // TODO: opaque handle allocator
        using Chunk          = util::OpaqueHandle<"voxel::ChunkRenderManager::Chunk", u16>;
        using RaytracedLight = util::OpaqueHandle<"voxel::ChunkRenderManager::RaytracedLight", u16>;
    public:
        explicit ChunkRenderManager(const game::Game*);
        ~ChunkRenderManager();

        ChunkRenderManager(const ChunkRenderManager&)             = delete;
        ChunkRenderManager(ChunkRenderManager&&)                  = delete;
        ChunkRenderManager& operator= (const ChunkRenderManager&) = delete;
        ChunkRenderManager& operator= (ChunkRenderManager&&)      = delete;

        /// Creates an empty chunk at the world aligned position
        [[nodiscard]] Chunk createChunk(WorldPosition);
        void                destroyChunk(Chunk);

        RaytracedLight createRaytracedLight(GpuRaytracedLight);
        void           destroyRaytracedLight(RaytracedLight);

        void updateChunk(
            const Chunk&,
            std::span<const ChunkLocalUpdate>,
            std::source_location = std::source_location::current());

        std::vector<game::FrameGenerator::RecordObject>
        processUpdatesAndGetDrawObjects(const game::Camera&);

    private:
        const game::Game* game;

        // Global data
        gfx::vulkan::WriteOnlyBuffer<GlobalVoxelData> global_voxel_data;

        // RayTracedLights
        util::OpaqueHandleAllocator<RaytracedLight>     raytraced_light_allocator;
        gfx::vulkan::CpuCachedBuffer<GpuRaytracedLight> raytraced_lights;

        // Per Chunk Data
        util::OpaqueHandleAllocator<Chunk>            chunk_id_allocator;
        std::vector<CpuChunkData>                     cpu_chunk_data;
        gfx::vulkan::CpuCachedBuffer<PerChunkGpuData> gpu_chunk_data;
        static constexpr std::size_t VramOverheadPerChunk = sizeof(PerChunkGpuData);

        // Per Brick Data
        util::RangeAllocator                                 brick_range_allocator;
        gfx::vulkan::WriteOnlyBuffer<BrickParentInformation> per_brick_chunk_parent_info;
        gfx::vulkan::CpuCachedBuffer<MaterialBrick>          material_bricks;
        gfx::vulkan::CpuCachedBuffer<BitBrick>               shadow_bricks;
        gfx::vulkan::WriteOnlyBuffer<VisibilityBrick>        visibility_bricks;
        static constexpr std::size_t                         VramOverheadPerBrick =
            sizeof(BrickParentInformation) + sizeof(MaterialBrick) + sizeof(BitBrick)
            + sizeof(BitBrick) + sizeof(VisibilityBrick);

        // Greedily Meshed Voxel Data
        util::RangeAllocator                          voxel_face_allocator;
        gfx::vulkan::WriteOnlyBuffer<GreedyVoxelFace> voxel_faces;

        gfx::vulkan::WriteOnlyBuffer<VisibleFaceIdBrickHashMapStorage> visible_face_id_map;
        gfx::vulkan::WriteOnlyBuffer<VisibleFaceData>                  visible_face_data;

        // Actual Draw Data

        struct ChunkDrawIndirectInstancePayload
        {
            u32 normal;
            u32 chunk_id;
        };
        gfx::vulkan::WriteOnlyBuffer<ChunkDrawIndirectInstancePayload> indirect_payload;
        gfx::vulkan::WriteOnlyBuffer<vk::DrawIndirectCommand>          indirect_commands;

        gfx::vulkan::WriteOnlyBuffer<VoxelMaterial> materials;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> voxel_chunk_descriptor_set_layout;

        std::shared_ptr<vk::UniquePipeline> voxel_chunk_render_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_visibility_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_calculation_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_transfer_pipeline;

        vk::DescriptorSet global_descriptor_set;
        vk::DescriptorSet voxel_chunk_descriptor_set;
    };
} // namespace voxel

// // dump updates
// // each frame
// // step along cycle:
// // collect updates -> process updates
// // States:
// // WaitingForUpdates
// // ProcessingUpdates