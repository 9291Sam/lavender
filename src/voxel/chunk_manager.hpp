#pragma once

#include "brick_map.hpp"
#include "brick_pointer_allocator.hpp"
#include "chunk.hpp"
#include "dense_bit_chunk.hpp"
#include "directional_face_id_brick.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/buffer_stager.hpp"
#include "gfx/vulkan/tracked_buffer.hpp"
#include "greedy_voxel_face.hpp"
#include "material_brick.hpp"
#include "point_light.hpp"
#include "util/index_allocator.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "visibility_brick.hpp"
#include "voxel.hpp"
#include "voxel/constants.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/opacity_brick.hpp"
#include <boost/container_hash/hash_fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <source_location>
#include <unordered_map>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    class ChunkManager
    {
    public:
        explicit ChunkManager(const game::Game*);
        ~ChunkManager();

        ChunkManager(const ChunkManager&)             = delete;
        ChunkManager(ChunkManager&&)                  = delete;
        ChunkManager& operator= (const ChunkManager&) = delete;
        ChunkManager& operator= (ChunkManager&&)      = delete;

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject>
        makeRecordObject(const game::Game*, const gfx::vulkan::BufferStager&, game::Camera);

        Chunk createChunk(ChunkCoordinate);
        void  destroyChunk(Chunk);

        PointLight createPointLight();
        void       modifyPointLight(
                  const PointLight&, glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs);
        void destroyPointLight(PointLight);

        void  writeVoxelToChunk(const Chunk&, ...);
        // TODO: many by default
        bool  readVoxelFromChunkOpacity(const Chunk&, );
        Voxel readVoxelFromChunkMaterial(const Chunk&, );

        void writeVoxel(glm::i32vec3, Voxel);

    private:

        Chunk allocateChunk(glm::ivec3 position);
        void  deallocateChunk(Chunk);

        void writeVoxelToChunk(
            const Chunk&,
            ChunkLocalPosition,
            Voxel,
            std::source_location = std::source_location::current());

        std::unique_ptr<DenseBitChunk> makeDenseBitChunk(u32 chunkId);

        const gfx::Renderer* renderer;

        struct CpuChunkData
        {
            glm::vec4                                           position;
            std::optional<std::array<util::RangeAllocation, 6>> face_data;
            bool                                                needs_remesh;
        };

        util::IndexAllocator      chunk_id_allocator;
        std::vector<CpuChunkData> cpu_chunk_data;

        struct GpuChunkData
        {
            glm::i32vec4 position;
        };
        gfx::vulkan::TrackedBuffer<GpuChunkData> gpu_chunk_data;
        gfx::vulkan::TrackedBuffer<BrickMap>     brick_maps;

        std::unordered_map<ChunkCoordinate, Chunk> global_chunks;

        struct ChunkDrawIndirectInstancePayload
        {
            glm::vec4 position; // TODO: what the fuck
            u32       normal;
            u32       chunk_id;
        };
        gfx::vulkan::Buffer<ChunkDrawIndirectInstancePayload> indirect_payload;
        gfx::vulkan::Buffer<vk::DrawIndirectCommand>          indirect_commands;

        struct BrickParentInformation
        {
            u32 parent_chunk             : 23; // actually only using 16
            u32 position_in_parent_chunk : 9;
        };
        BrickPointerAllocator                              brick_allocator;
        gfx::vulkan::TrackedBuffer<BrickParentInformation> brick_parent_info;
        gfx::vulkan::TrackedBuffer<MaterialBrick>          material_bricks;
        gfx::vulkan::TrackedBuffer<OpacityBrick>           opacity_bricks;
        gfx::vulkan::Buffer<VisibilityBrick>               visibility_bricks;
        gfx::vulkan::Buffer<DirectionalFaceIdBrick>        face_id_bricks;

        util::RangeAllocator                 voxel_face_allocator;
        gfx::vulkan::Buffer<GreedyVoxelFace> voxel_faces;

        gfx::vulkan::Buffer<VoxelMaterial> material_buffer;

        struct GlobalVoxelData
        {
            u32 number_of_visible_faces;
            u32 number_of_calculating_draws_x;
            u32 number_of_calculating_draws_y;
            u32 number_of_calculating_draws_z;
            u32 number_of_lights;
        };

        struct VisibleFaceData
        {
            u32       data;
            glm::vec3 calculated_color;
        };
        gfx::vulkan::Buffer<GlobalVoxelData> global_voxel_data;
        gfx::vulkan::Buffer<VisibleFaceData> visible_face_data;

        util::IndexAllocator                           light_allocator;
        gfx::vulkan::TrackedBuffer<InternalPointLight> lights_buffer;

        gfx::vulkan::TrackedBuffer<std::array<std::array<std::array<u16, 256>, 256>, 256>>
            global_chunks_buffer;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> descriptor_set_layout;

        std::shared_ptr<vk::UniquePipeline> voxel_render_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_visibility_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_calculation_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_transfer_pipeline;

        vk::DescriptorSet chunk_descriptor_set;
        vk::DescriptorSet global_descriptor_set;
    };
} // namespace voxel