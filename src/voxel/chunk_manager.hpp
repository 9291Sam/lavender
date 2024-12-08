#pragma once

#include "brick_map.hpp"
#include "chunk.hpp"
#include "dense_bit_chunk.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
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
#include <boost/dynamic_bitset.hpp>
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    class ChunkManager
    {
    public:
        struct VoxelWrite
        {
            ChunkLocalPosition position;
            Voxel              voxel;
        };

    public:
        explicit ChunkManager(const game::Game*);
        ~ChunkManager();

        ChunkManager(const ChunkManager&)             = delete;
        ChunkManager(ChunkManager&&)                  = default;
        ChunkManager& operator= (const ChunkManager&) = delete;
        ChunkManager& operator= (ChunkManager&&)      = default;

        [[nodiscard]] std::vector<game::FrameGenerator::RecordObject> makeRecordObject(
            const game::Game*,
            const gfx::vulkan::BufferStager&,
            game::Camera,
            gfx::profiler::TaskGenerator&);

        Chunk createChunk(ChunkCoordinate);
        void  destroyChunk(Chunk);

        PointLight createPointLight();
        void       modifyPointLight(
                  const PointLight&, glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs);
        void destroyPointLight(PointLight);

        void writeVoxelToChunk(const Chunk&, std::span<const VoxelWrite>);

        bool areAnyVoxelsOccupied(const Chunk&, std::span<const ChunkLocalPosition>);
        [[nodiscard]] boost::dynamic_bitset<u64>
        readVoxelFromChunkOpacity(const Chunk&, std::span<const ChunkLocalPosition>);
        std::vector<Voxel>
        readVoxelFromChunkMaterial(const Chunk&, std::span<const ChunkLocalPosition>);

    private:

        struct BrickList
        {
            std::vector<std::pair<BrickCoordinate, OpacityBrick>> data;

            [[nodiscard]] std::unique_ptr<DenseBitChunk> formDenseBitChunk() const
            {
                std::unique_ptr<DenseBitChunk> out = std::make_unique<DenseBitChunk>();

                for (const auto& [bC, thisBrick] : this->data)
                {
                    thisBrick.iterateOverVoxels(
                        [&](BrickLocalPosition bP, bool isFilled)
                        {
                            ChunkLocalPosition pos = assembleChunkLocalPosition(bC, bP);

                            if (isFilled)
                            {
                                // NOLINTNEXTLINE
                                out->data[pos.x][pos.y] |= (1ULL << pos.z);
                            }
                        });
                }

                return out;
            }
        };

        BrickList formBrickList(u32 chunkId);

        const gfx::Renderer* renderer;

        struct CpuChunkData
        {
            glm::vec4                                                       position;
            std::optional<std::array<util::RangeAllocation, 6>>             face_data;
            bool                                                            needs_remesh;
            bool                                                            is_remesh_in_progress;
            std::shared_future<std::array<std::vector<GreedyVoxelFace>, 6>> future_mesh;
        };

        util::IndexAllocator      chunk_id_allocator;
        std::vector<CpuChunkData> cpu_chunk_data;

        struct GpuChunkData
        {
            glm::i32vec4 position;
        };
        gfx::vulkan::CpuCachedBuffer<GpuChunkData> gpu_chunk_data;
        gfx::vulkan::CpuCachedBuffer<BrickMap>     brick_maps;

        struct ChunkDrawIndirectInstancePayload
        {
            glm::vec4 position; // TODO: what the fuck
            u32       normal;
            u32       chunk_id;
        };
        gfx::vulkan::WriteOnlyBuffer<ChunkDrawIndirectInstancePayload> indirect_payload;
        gfx::vulkan::WriteOnlyBuffer<vk::DrawIndirectCommand>          indirect_commands;

        struct BrickParentInformation
        {
            u32 parent_chunk             : 23; // actually only using 16
            u32 position_in_parent_chunk : 9;
        };
        util::IndexAllocator                                 brick_allocator;
        gfx::vulkan::CpuCachedBuffer<BrickParentInformation> brick_parent_info;
        gfx::vulkan::CpuCachedBuffer<MaterialBrick>          material_bricks;
        gfx::vulkan::CpuCachedBuffer<OpacityBrick>           opacity_bricks;
        gfx::vulkan::WriteOnlyBuffer<VisibilityBrick>        visibility_bricks;
        // gfx::vulkan::WriteOnlyBuffer<DirectionalFaceIdBrick> face_id_bricks;

        struct FaceIdBrickHashMapStorage
        {
            u32 key;
            u32 value;
        };
        gfx::vulkan::WriteOnlyBuffer<FaceIdBrickHashMapStorage> face_id_map;

        util::RangeAllocator                          voxel_face_allocator;
        gfx::vulkan::CpuCachedBuffer<GreedyVoxelFace> voxel_faces;

        gfx::vulkan::WriteOnlyBuffer<VoxelMaterial> material_buffer;

        struct GlobalVoxelData
        {
            u32 number_of_visible_faces;
            u32 number_of_calculating_draws_x;
            u32 number_of_calculating_draws_y;
            u32 number_of_calculating_draws_z;
            u32 number_of_lights;
            u32 readback_number_of_visible_faces;
        };

        struct VisibleFaceData
        {
            u32       data;
            glm::vec3 calculated_color;
        };
        gfx::vulkan::WriteOnlyBuffer<GlobalVoxelData> global_voxel_data;
        gfx::vulkan::WriteOnlyBuffer<VisibleFaceData> visible_face_data;

        util::IndexAllocator                             light_allocator;
        gfx::vulkan::CpuCachedBuffer<InternalPointLight> lights_buffer;

        gfx::vulkan::CpuCachedBuffer<std::array<std::array<std::array<u16, 256>, 256>, 256>>
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
