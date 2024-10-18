#pragma once

#include "brick_map.hpp"
#include "brick_pointer_allocator.hpp"
#include "chunk.hpp"
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
#include "voxel/material_manager.hpp"
#include "voxel/opacity_brick.hpp"
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <source_location>
#include <unordered_map>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    struct ChunkSlice
    {
        // width is within each u64, height is the index
        std::array<u64, 64> data;
    };

    struct CpuChunkData
    {
        glm::vec4                                           position;
        std::optional<std::array<util::RangeAllocation, 6>> face_data;
        bool                                                needs_remesh;
        std::array<std::array<ChunkSlice, 64>, 6>           slice_data;
    };

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
        makeRecordObject(
            const game::Game*, const gfx::vulkan::BufferStager&, game::Camera);

        PointLight createPointLight();
        void       destroyPointLight(PointLight);

        void modifyPointLight(
            const PointLight&,
            glm::vec3 position,
            glm::vec4 colorAndPower,
            glm::vec4 falloffs);

        void writeVoxel(glm::i32vec3, Voxel);

    private:

        Chunk allocateChunk(glm::ivec3 position);
        void  deallocateChunk(Chunk);

        void writeVoxelToChunk(
            const Chunk&,
            ChunkLocalPosition,
            Voxel,
            std::source_location = std::source_location::current());

        std::array<util::RangeAllocation, 6> meshChunkNormal(u32 chunkId);
        std::array<util::RangeAllocation, 6> meshChunkLinear(u32 chunkId);
        std::array<util::RangeAllocation, 6> meshChunkGreedy(u32 chunkId);

        struct DenseBitChunk
        {
            std::array<std::array<u64, 64>, 64> data;

            static bool isPositionInBounds(glm::i8vec3 p)
            {
                return p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                    && p.z < 64;
            }

            // returns false on out of bounds access
            [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
            {
                // util::assertFatal(
                //     p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                //         && p.z < 64,
                //     "{} {} {}",
                //     p.x,
                //     p.y,
                //     p.z);

                if (p.x < 0 || p.x >= 64 || p.y < 0 || p.y >= 64 || p.z < 0
                    || p.z >= 64)
                {
                    return false;
                }
                else
                {
                    return static_cast<bool>(
                        this->data[static_cast<std::size_t>(p.x)] // NOLINT
                                  [static_cast<std::size_t>(p.y)] // NOLINT
                        & (1ULL << static_cast<u64>(p.z)));
                }
            }
            // if its occupied, it removes it from the data structure
            [[nodiscard]] bool isOccupiedClearing(glm::i8vec3 p)
            {
                const bool result = this->isOccupied(p);

                if (result)
                {
                    this->write(p, false);
                }

                return result;
            }

            [[nodiscard]] bool isEntireRangeOccupied(
                glm::i8vec3 base, glm::i8vec3 step, i8 range) const // NOLINT
            {
                bool isEntireRangeOccupied = true;

                for (i8 i = 0; i < range; ++i)
                {
                    glm::i8vec3 probe = base + step * i;

                    if (!this->isOccupied(probe))
                    {
                        isEntireRangeOccupied = false;
                        break;
                    }
                }

                return isEntireRangeOccupied;
            }

            void clearEntireRange(glm::i8vec3 base, glm::i8vec3 step, i8 range)
            {
                for (i8 i = 0; i < range; ++i)
                {
                    glm::i8vec3 probe = base + step * i;

                    this->write(probe, false);
                }
            }

            void write(glm::i8vec3 p, bool filled)
            {
                // if constexpr (util::isDebugBuild())
                // {
                util::assertFatal(
                    p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0
                        && p.z < 64,
                    "{} {} {}",
                    p.x,
                    p.y,
                    p.z);
                // }

                if (filled)
                {
                    // NOLINTNEXTLINE
                    this->data[static_cast<std::size_t>(p.x)]
                              [static_cast<std::size_t>(p.y)] |=
                        (u64 {1} << static_cast<u64>(p.z));
                }
                else
                {
                    // NOLINTNEXTLINE
                    this->data[static_cast<std::size_t>(p.x)]
                              [static_cast<std::size_t>(p.y)] &=
                        ~(u64 {1} << static_cast<u64>(p.z));
                }
            }
        };

        std::unique_ptr<DenseBitChunk> makeDenseBitChunk(u32 chunkId);

        const gfx::Renderer* renderer;

        util::IndexAllocator      chunk_id_allocator;
        std::vector<CpuChunkData> cpu_chunk_data;
        struct GpuChunkData // TODO: modify this so the adjacent chunks are
                            // uploaded
        {
            glm::i32vec4                                     position;
            std::array<std::array<std::array<u32, 3>, 3>, 3> adjacent_chunks;
            u32                                              padding;
        };
        gfx::vulkan::TrackedBuffer<GpuChunkData> gpu_chunk_data;
        gfx::vulkan::TrackedBuffer<BrickMap>     brick_maps;

        std::unordered_map<ChunkCoordinate, Chunk> global_chunks;

        struct ChunkDrawIndirectInstancePayload
        {
            glm::vec4 position;
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

        struct VisibleVoxelFaces
        {
            u32 number_of_visible_faces;
            u32 number_of_calculating_draws_x;
            u32 number_of_calculating_draws_y;
            u32 number_of_calculating_draws_z;
        };

        struct VisibleFaceData
        {
            u32       data;
            glm::vec3 calculated_color;
        };
        gfx::vulkan::Buffer<VisibleVoxelFaces> number_of_visible_faces;
        gfx::vulkan::Buffer<VisibleFaceData>   visible_face_data;

        struct InternalPointLight
        {
            glm::vec4 position;
            glm::vec4 color_and_power;
            glm::vec4 falloffs;
        };
        struct PointLightStorage
        {
            u32                                  number_of_point_lights;
            u32                                  pad0;
            u32                                  pad1;
            u32                                  pad2;
            std::array<InternalPointLight, 1280> lights;
        };
        gfx::vulkan::Buffer<PointLightStorage>      point_lights;
        util::IndexAllocator                        point_light_allocator;
        std::unordered_map<u32, InternalPointLight> point_light_id_payload;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> descriptor_set_layout;

        std::shared_ptr<vk::UniquePipeline> voxel_render_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_visibility_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_calculation_pipeline;
        std::shared_ptr<vk::UniquePipeline> voxel_color_transfer_pipeline;

        vk::DescriptorSet chunk_descriptor_set;
        vk::DescriptorSet global_descriptor_set;
    };
} // namespace voxel
