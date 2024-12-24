#include "chunk_render_manager.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/window.hpp"
#include "structures.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "util/static_filesystem.hpp"
#include "util/thread_pool.hpp"
#include "util/timer.hpp"
#include "voxel/material_manager.hpp"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <future>
#include <glm/geometric.hpp>
#include <memory>
#include <ranges>
#include <source_location>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    namespace
    {

        struct DenseBitChunk
        {
            std::array<std::array<u64, VoxelsPerChunkEdge>, VoxelsPerChunkEdge> data;

            static bool isPositionInBounds(glm::i8vec3 p)
            {
                return p.x >= 0 && p.x < VoxelsPerChunkEdge && p.y >= 0 && p.y < VoxelsPerChunkEdge
                    && p.z >= 0 && p.z < VoxelsPerChunkEdge;
            }

            // returns false on out of bounds access
            [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
            {
                if (p.x < 0 || p.x >= VoxelsPerChunkEdge || p.y < 0 || p.y >= VoxelsPerChunkEdge
                    || p.z < 0 || p.z >= VoxelsPerChunkEdge)
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
        };

        struct BrickList
        {
            std::vector<std::pair<BrickCoordinate, BitBrick>> data;

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

        std::array<std::vector<GreedyVoxelFace>, 6>
        meshChunkGreedy(std::unique_ptr<DenseBitChunk> thisChunkData) // NOLINT
        {
            std::array<std::vector<GreedyVoxelFace>, 6> outFaces {};

            struct ChunkSlice
            {
                // width is within each u64, height is the index
                std::array<u64, 64> data;
            };

            auto makeChunkSlice = [&](u32 normalId, u64 offset) -> ChunkSlice
            {
                VoxelFaceDirection dir = static_cast<VoxelFaceDirection>(normalId);
                const auto [widthAxis, heightAxis, ascensionAxis] = getDrivingAxes(dir);
                glm::i8vec3 normal                                = getDirFromDirection(dir);

                ChunkSlice res {}; // NOLINT

                for (i8 h = 0; h < 64; ++h)
                {
                    for (i8 w = 0; w < 64; ++w)
                    {
                        if (thisChunkData->isOccupied(
                                w * widthAxis + h * heightAxis
                                + static_cast<i8>(offset) * ascensionAxis))
                        {
                            if (DenseBitChunk::isPositionInBounds(
                                    w * widthAxis + h * heightAxis
                                    + static_cast<i8>(offset) * ascensionAxis + normal)
                                && thisChunkData->isOccupied(
                                    w * widthAxis + h * heightAxis
                                    + static_cast<i8>(offset) * ascensionAxis + normal))
                            {
                                continue;
                            }
                            else
                            {
                                // NOLINTNEXTLINE
                                res.data[static_cast<std::size_t>(h)] |=
                                    (UINT64_C(1) << static_cast<u64>(w));
                            }
                        }
                    }
                }

                return res;
            };
            u32 normalId = 0;
            for (std::vector<GreedyVoxelFace>& faces : outFaces)
            {
                for (u64 ascend = 0; ascend < 64; ++ascend)
                {
                    ChunkSlice thisSlice = makeChunkSlice(normalId, ascend);

                    for (u64 height = 0; height < 64; ++height)
                    {
                        // NOLINTNEXTLINE
                        for (u64 width = static_cast<u64>(std::countr_zero(thisSlice.data[height]));
                             width < 64;
                             ++width)
                        {
                            // NOLINTNEXTLINE
                            if ((thisSlice.data[height] & (UINT64_C(1) << width)) != 0ULL)
                            {
                                const u64 faceWidth = static_cast<u64>(std::countr_one(
                                    // NOLINTNEXTLINE
                                    thisSlice.data[height] >> width));

                                VoxelFaceDirection dir = static_cast<VoxelFaceDirection>(normalId);
                                const auto [widthAxis, heightAxis, ascensionAxis] =
                                    getDrivingAxes(dir);

                                glm::i8vec3 thisRoot = ascensionAxis * static_cast<i8>(ascend)
                                                     + heightAxis * static_cast<i8>(height)
                                                     + widthAxis * static_cast<i8>(width);

                                u64 mask = 0;

                                if (faceWidth == 64)
                                {
                                    mask = ~0ULL;
                                }
                                else
                                {
                                    mask = ((1ULL << faceWidth) - 1ULL) << width;
                                }

                                u64 faceHeight = 0;
                                for (u64 h = height; h < 64; ++h)
                                {
                                    // NOLINTNEXTLINE
                                    if ((thisSlice.data[h] & mask) == mask)
                                    {
                                        faceHeight += 1;
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                for (u64 h = height; h < (height + faceHeight); ++h)
                                {
                                    // NOLINTNEXTLINE
                                    thisSlice.data[h] &= ~mask;
                                }

                                faces.push_back(GreedyVoxelFace {
                                    .x {static_cast<u32>(thisRoot.x)},
                                    .y {static_cast<u32>(thisRoot.y)},
                                    .z {static_cast<u32>(thisRoot.z)},
                                    .width {static_cast<u32>(faceWidth - 1)},
                                    .height {static_cast<u32>(faceHeight - 1)},
                                    .pad {0}});

                                width += faceWidth;
                            }
                            else
                            {
                                // NOLINTNEXTLINE
                                width += static_cast<u64>(
                                    std::countr_zero(
                                        // NOLINTNEXTLINE
                                        thisSlice.data[height] >> width)
                                    - 1);
                            }
                        }
                    }
                }

                normalId += 1;
            }

            return outFaces;
        }

        [[nodiscard]] ChunkAsyncMesh doMesh(
            const u16                              chunkId,
            const PerChunkGpuData                  oldGpuData,
            const std::span<const MaterialBrick>   oldMaterialBricks,
            const std::span<const ShadowBrick>     oldShadowBricks,
            const std::span<const PrimaryRayBrick> oldPrimaryRayBricks,
            // NOLINTNEXTLINE(performance-unnecessary-value-param) lifetimes exist
            const std::vector<ChunkLocalUpdate>    newUpdates)
        {
            util::MultiTimer t {};

            util::IndexAllocator newChunkOffsetAllocator {
                BricksPerChunkEdge * BricksPerChunkEdge * BricksPerChunkEdge};
            ChunkBrickMap                       newBrickMap {};
            std::vector<BrickParentInformation> newParentBricks {};
            std::vector<MaterialBrick>          newMaterialBricks {};
            std::vector<ShadowBrick>            newShadowBricks {};
            std::vector<PrimaryRayBrick>        newPrimaryRayBricks {};

            // Propagate old updates
            oldGpuData.data.iterateOverBricks(
                [&](BrickCoordinate bC, u16 oldOffset)
                {
                    if (oldOffset != ChunkBrickMap::NullOffset
                        && oldMaterialBricks[oldOffset].isSolid()
                               != Voxel::NullAirEmpty) // TODO: do proper dense
                                                       // brick things!
                    {
                        const u16 newOffset =
                            static_cast<u16>(newChunkOffsetAllocator.allocateOrPanic());

                        newBrickMap.setOffset(bC, newOffset);

                        newParentBricks.push_back(BrickParentInformation {
                            .parent_chunk {chunkId},
                            .position_in_parent_chunk {static_cast<u32>(bC.asLinearIndex())}});
                        newMaterialBricks.push_back(oldMaterialBricks[oldOffset]);
                        newShadowBricks.push_back(oldShadowBricks[oldOffset]);
                        newPrimaryRayBricks.push_back(oldPrimaryRayBricks[oldOffset]);
                    }
                });

            t.stamp("propagate old updates");

            for (const ChunkLocalUpdate& newUpdate : newUpdates)
            {
                const ChunkLocalPosition             updatePosition = newUpdate.getPosition();
                const Voxel                          updateVoxel    = newUpdate.getVoxel();
                const ChunkLocalUpdate::ShadowUpdate shadowUpdate   = newUpdate.getShadowUpdate();
                const ChunkLocalUpdate::CameraVisibleUpdate cameraVisibilityUpdate =
                    newUpdate.getCameraVisibility();

                const auto [coordinate, local] = splitChunkLocalPosition(updatePosition);

                u16 maybeOffset = newBrickMap.getOffset(coordinate);
                if (maybeOffset == ChunkBrickMap::NullOffset)
                {
                    maybeOffset = static_cast<u16>(newChunkOffsetAllocator.allocateOrPanic());

                    newBrickMap.setOffset(coordinate, maybeOffset);

                    newParentBricks.push_back(BrickParentInformation {
                        .parent_chunk {chunkId},
                        .position_in_parent_chunk {static_cast<u32>(coordinate.asLinearIndex())}});
                    newMaterialBricks.push_back(MaterialBrick {});
                    newShadowBricks.push_back(ShadowBrick {});
                    newPrimaryRayBricks.push_back(PrimaryRayBrick {});
                }
                newMaterialBricks[maybeOffset].write(local, updateVoxel);
                newShadowBricks[maybeOffset].write(local, static_cast<bool>(shadowUpdate));
                newPrimaryRayBricks[maybeOffset].write(
                    local, static_cast<bool>(cameraVisibilityUpdate));
            }

            t.stamp("propagate new updates");

            BrickList list {};

            newBrickMap.iterateOverBricks(
                [&](BrickCoordinate bC, u16 maybeOffset)
                {
                    if (maybeOffset != ChunkBrickMap::NullOffset)
                    {
                        list.data.push_back(
                            {bC, static_cast<BitBrick>(newPrimaryRayBricks[maybeOffset])});
                    }
                });

            std::array<std::vector<GreedyVoxelFace>, 6> newGreedyFaces =
                meshChunkGreedy(list.formDenseBitChunk());

            t.stamp("form brick list and do mesh");

            std::ignore = t.finish();

            return ChunkAsyncMesh {
                .new_brick_map {newBrickMap},
                .new_parent_bricks {std::move(newParentBricks)},
                .new_material_bricks {std::move(newMaterialBricks)},
                .new_shadow_bricks {std::move(newShadowBricks)},
                .new_primary_ray_bricks {std::move(newPrimaryRayBricks)},
                .new_greedy_faces {std::move(newGreedyFaces)}};
        }

    } // namespace

    static constexpr u32 MaxChunks          = 4096; // max of u16
    static constexpr u32 DirectionsPerChunk = 6;
    static constexpr u32 MaxBricks          = 1U << 20U; // FIXED(shader bound)
    static constexpr u32 MaxFaces           = 1U << 23U; // FIXED(shader bound)
    static constexpr u32 MaxFaceIdHashNodes = 1U << 23U; // FIXED(shader bound)
    static constexpr u32 MaxLights          = 4096;

    ChunkRenderManager::ChunkRenderManager(const game::Game* game_)
        : game {game_}
        , global_voxel_data(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
                  | vk::BufferUsageFlagBits::eIndirectBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Global Voxel Data")
        , raytraced_light_allocator {MaxLights}
        , raytraced_lights(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxLights,
              "Gpy Raytraced Lights Buffer")
        , chunk_id_allocator {MaxChunks}
        , gpu_chunk_data(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxChunks,
              "Gpu Chunk Data Buffer")
        , brick_range_allocator(MaxBricks, MaxBricks * 2)
        , per_brick_chunk_parent_info(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Brick Parent Info")
        , material_bricks(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Material Bricks")
        , shadow_bricks(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Shadow Bricks")
        , visibility_bricks(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Visibility Bricks")
        , primary_ray_bricks(static_cast<std::size_t>(MaxBricks), PrimaryRayBrick {})
        , voxel_face_allocator(MaxFaces, MaxChunks * 6)
        , voxel_faces(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaces),
              "Voxel Faces")
        , visible_face_id_map(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaceIdHashNodes),
              "Face Id Map")
        , visible_face_data(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaces),
              "Visible Face Data")
        , indirect_payload(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Payload")
        , indirect_commands(
              game_->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Commands")
        , materials(generateVoxelMaterialBuffer(game_->getRenderer()))
        , voxel_chunk_descriptor_set_layout(
              game_->getRenderer()->getAllocator()->cacheDescriptorSetLayout(
                  gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {
                      .bindings {{
                          // GlobalVoxelData
                          vk::DescriptorSetLayoutBinding {
                              .binding {0},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // RaytracedLights
                          vk::DescriptorSetLayoutBinding {
                              .binding {1},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // PerChunkGpuData
                          vk::DescriptorSetLayoutBinding {
                              .binding {2},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // PerBrickChunkParentInfo
                          vk::DescriptorSetLayoutBinding {
                              .binding {3},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // MaterialBricks
                          vk::DescriptorSetLayoutBinding {
                              .binding {4},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // ShadowBricks
                          vk::DescriptorSetLayoutBinding {
                              .binding {5},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // VisibilityBricks
                          vk::DescriptorSetLayoutBinding {
                              .binding {6},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // VoxelFaces
                          vk::DescriptorSetLayoutBinding {
                              .binding {7},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // VisibleFaceIdMap
                          vk::DescriptorSetLayoutBinding {
                              .binding {8},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // VisibleFaceData
                          vk::DescriptorSetLayoutBinding {
                              .binding {9},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                          // Materials
                          vk::DescriptorSetLayoutBinding {
                              .binding {10},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment
                                  | vk::ShaderStageFlagBits::eCompute},
                              .pImmutableSamplers {nullptr},
                          },
                      }},
                      .name {"Voxel Descriptor Set Layout"}}))
        , voxel_chunk_render_pipeline {game_->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_render.vert"),
                              "Voxel Render Vertex Shader")},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_render.frag"),
                              "Voxel Render Fragment Shader")},
                          .entry_point {"main"}},
                  }},
                  .vertex_attributes {{
                      vk::VertexInputAttributeDescription {
                          .location {0},
                          .binding {0},
                          .format {vk::Format::eR32Uint},
                          .offset {offsetof(ChunkDrawIndirectInstancePayload, normal)}},
                      vk::VertexInputAttributeDescription {
                          .location {1},
                          .binding {0},
                          .format {vk::Format::eR32Uint},
                          .offset {offsetof(ChunkDrawIndirectInstancePayload, chunk_id)}},
                  }},
                  .vertex_bindings {{vk::VertexInputBindingDescription {
                      .binding {0},
                      .stride {sizeof(ChunkDrawIndirectInstancePayload)},
                      .inputRate {vk::VertexInputRate::eInstance}}}},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .discard_enable {false},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eBack},
                  .front_face {vk::FrontFace::eCounterClockwise},
                  .depth_test_enable {true},
                  .depth_write_enable {true},
                  .depth_compare_op {vk::CompareOp::eLess},
                  .color_format {vk::Format::eR32Uint},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .blend_enable {false},
                  .layout {game_->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game_->getGlobalInfoDescriptorSetLayout(),
                               this->voxel_chunk_descriptor_set_layout}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {vk::ShaderStageFlagBits::eVertex},
                              .offset {0},
                              .size {sizeof(u32)},
                          }},
                          .name {"Voxel Render Pipeline Layout"}})},
                  .name {"Voxel Render Pipeline"}})}
        , voxel_visibility_pipeline {game_->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                      staticFilesystem::loadShader("voxel_visibility_detection.comp"),
                      "Voxel Visibility Detection Compute Shader")},
                  .layout {game_->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game_->getGlobalInfoDescriptorSetLayout(),
                               this->voxel_chunk_descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Visibility Detection Pipeline Layout"}})},
                  .name {"Voxel Visibility Detection Pipeline"}})}
        , voxel_color_calculation_pipeline {game_->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                      staticFilesystem::loadShader("voxel_color_calculation.comp"),
                      "Voxel Color Calculation Compute Shader")},
                  .layout {game_->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game_->getGlobalInfoDescriptorSetLayout(),
                               this->voxel_chunk_descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Color Calculation Pipeline Layout"}})},
                  .name {"Voxel Color Calculation Pipeline"},
              })}
        , voxel_color_transfer_pipeline {game_->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_color_transfer.vert"),
                              "Voxel Color Transfer Vertex "
                              "Shader")},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_color_transfer."
                                                           "frag"),
                              "Voxel Color Transfer Fragment "
                              "Shader")},
                          .entry_point {"main"}},
                  }},
                  .vertex_attributes {},
                  .vertex_bindings {},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .discard_enable {false},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eCounterClockwise},
                  .depth_test_enable {false},
                  .depth_write_enable {false},
                  .depth_compare_op {vk::CompareOp::eNever},
                  .color_format {gfx::Renderer::ColorFormat.format},
                  .depth_format {},
                  .blend_enable {false},
                  .layout {game_->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game_->getGlobalInfoDescriptorSetLayout(),
                               this->voxel_chunk_descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Color Transfer Pipeline Layout"}})},
                  .name {"Voxel Color Transfer Pipeline"}})}
        , global_descriptor_set {game_->getGlobalInfoDescriptorSet()}
        , voxel_chunk_descriptor_set {game_->getRenderer()->getAllocator()->allocateDescriptorSet(
              **this->voxel_chunk_descriptor_set_layout, "Voxel Descriptor Set")}

    {
        const auto bufferInfo = {
            vk::DescriptorBufferInfo {
                .buffer {*this->global_voxel_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->raytraced_lights},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->gpu_chunk_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->per_brick_chunk_parent_info},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->material_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->shadow_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visibility_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->voxel_faces},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visible_face_id_map},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visible_face_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->materials},
                .offset {0},
                .range {vk::WholeSize},
            }};

        std::vector<vk::WriteDescriptorSet> writes {};

        u32 idx = 0;
        for (const vk::DescriptorBufferInfo& i : bufferInfo)
        {
            writes.push_back(vk::WriteDescriptorSet {
                .sType {vk::StructureType::eWriteDescriptorSet},
                .pNext {nullptr},
                .dstSet {this->voxel_chunk_descriptor_set},
                .dstBinding {idx},
                .dstArrayElement {0},
                .descriptorCount {1},
                .descriptorType {vk::DescriptorType::eStorageBuffer},
                .pImageInfo {nullptr},
                .pBufferInfo {&i},
                .pTexelBufferView {nullptr},
            });

            idx += 1;
        }

        game_->getRenderer()->getDevice()->getDevice().updateDescriptorSets(
            static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

        this->cpu_chunk_data.resize(MaxChunks);
        this->visible_face_id_map.fillImmediate(
            VisibleFaceIdBrickHashMapStorage {.key {~0U}, .value {~0U}}); // TODO: remove?
        this->visibility_bricks.fillImmediate(VisibilityBrick {});

        util::logTrace("Constructed Chunk Render Manager");
    }

    ChunkRenderManager::~ChunkRenderManager() = default;

    ChunkRenderManager::Chunk ChunkRenderManager::createChunk(WorldPosition newChunkWorldPosition)
    {
        Chunk     newChunk = this->chunk_id_allocator.allocateOrPanic();
        const u16 chunkId  = this->chunk_id_allocator.getValueOfHandle(newChunk);

        this->cpu_chunk_data[chunkId] = CpuChunkData {};
        const PerChunkGpuData newChunkGpuData {
            .world_offset_x {newChunkWorldPosition.x},
            .world_offset_y {newChunkWorldPosition.y},
            .world_offset_z {newChunkWorldPosition.z},
            .brick_allocation_offset {0},
            .data {}};
        this->gpu_chunk_data.write(chunkId, newChunkGpuData);

        return newChunk;
    }

    void ChunkRenderManager::destroyChunk(Chunk chunk)
    {
        CpuChunkData& thisCpuChunkData =
            this->cpu_chunk_data[this->chunk_id_allocator.getValueOfHandle(chunk)];

        this->chunk_id_allocator.free(std::move(chunk));

        if (std::optional allocation = thisCpuChunkData.active_brick_range_allocation)
        {
            this->brick_range_allocator.free(*allocation);
        }

        if (std::optional faces = thisCpuChunkData.active_draw_allocations)
        {
            for (util::RangeAllocation a : *faces)
            {
                this->voxel_face_allocator.free(a);
            }
        }

        thisCpuChunkData = {};
    }

    ChunkRenderManager::RaytracedLight
    ChunkRenderManager::createRaytracedLight(GpuRaytracedLight rawLight)
    {
        RaytracedLight newRaytracedLight = this->raytraced_light_allocator.allocateOrPanic();

        this->raytraced_lights.write(
            this->raytraced_light_allocator.getValueOfHandle(newRaytracedLight), rawLight);

        return newRaytracedLight;
    }

    void ChunkRenderManager::destroyRaytracedLight(RaytracedLight light)
    {
        this->raytraced_lights.write(
            this->raytraced_light_allocator.getValueOfHandle(light), GpuRaytracedLight {});

        this->raytraced_light_allocator.free(std::move(light));
    }

    void ChunkRenderManager::updateChunk(
        const Chunk&                      chunk,
        std::span<const ChunkLocalUpdate> chunkUpdates,
        std::source_location              location)
    {
        util::assertFatal<>(!chunk.isNull(), "Tried to update null chunk!", location);
        util::assertWarn<>(!chunkUpdates.empty(), "Tried to do zero updates to a chunk!", location);

        std::vector<ChunkLocalUpdate>& updatesQueue =
            this->cpu_chunk_data[this->chunk_id_allocator.getValueOfHandle(chunk)].updates;

        // this is literally a 3x improvement over a loop
        updatesQueue.append_range(chunkUpdates);
    }

    std::vector<game::FrameGenerator::RecordObject>
    ChunkRenderManager::processUpdatesAndGetDrawObjects(
        const game::Camera& camera, gfx::profiler::TaskGenerator& profilerTaskGenerator) // NOLINT
    {
        const gfx::vulkan::BufferStager& stager = this->game->getRenderer()->getStager();

        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        u32 callNumber         = 0;
        u32 numberOfTotalFaces = 0;

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&](const u16 chunkId) // NOLINT
            {
                CpuChunkData& thisChunkData = this->cpu_chunk_data[chunkId];

                const glm::vec3 chunkPosition = [&]
                {
                    const PerChunkGpuData& gpuData = this->gpu_chunk_data.read(chunkId);

                    return glm::vec3 {
                        gpuData.world_offset_x,
                        gpuData.world_offset_y,
                        gpuData.world_offset_z,
                    };
                }();

                // we need to spawn a new mesh task
                if (!thisChunkData.updates.empty() && !thisChunkData.maybe_async_mesh.valid())
                {
                    const PerChunkGpuData oldGpuData = this->gpu_chunk_data.read(chunkId);

                    const std::size_t oldBricksPerChunk = [&]() -> std::size_t
                    {
                        const std::optional<u16> maxValidOffset =
                            oldGpuData.data.getMaxValidOffset();

                        if (maxValidOffset.has_value())
                        {
                            return *maxValidOffset + 1;
                        }
                        else
                        {
                            return 0;
                        }
                    }();

                    const std::span<const MaterialBrick> spanOldMaterialBricks =
                        this->material_bricks.read(
                            oldGpuData.brick_allocation_offset, oldBricksPerChunk);
                    const std::span<const ShadowBrick> spanOldShadowBricks =
                        this->shadow_bricks.read(
                            oldGpuData.brick_allocation_offset, oldBricksPerChunk);
                    const std::span<const PrimaryRayBrick> spanOldPrimaryRayBricks {
                        &this->primary_ray_bricks[oldGpuData.brick_allocation_offset],
                        oldBricksPerChunk};

                    thisChunkData.maybe_async_mesh = util::runAsync(
                        [chunkId,
                         localOldGpuData          = oldGpuData,
                         localOldMaterialBricks   = spanOldMaterialBricks,
                         localOldShadowBricks     = spanOldShadowBricks,
                         localOldPrimaryRayBricks = spanOldPrimaryRayBricks,
                         localNewUpdates          = std::move(thisChunkData.updates)]
                        {
                            return doMesh(
                                chunkId,
                                localOldGpuData,
                                localOldMaterialBricks,
                                localOldShadowBricks,
                                localOldPrimaryRayBricks,
                                localNewUpdates);
                        });
                }
                else if (
                    thisChunkData.maybe_async_mesh.valid()
                    && thisChunkData.maybe_async_mesh.wait_for(std::chrono::years {0})
                           == std::future_status::ready)
                {
                    if (thisChunkData.active_brick_range_allocation.has_value())
                    {
                        this->brick_range_allocator.free(
                            *thisChunkData.active_brick_range_allocation);
                    }

                    if (thisChunkData.active_draw_allocations.has_value())
                    {
                        for (util::RangeAllocation allocation :
                             *thisChunkData.active_draw_allocations)
                        {
                            this->voxel_face_allocator.free(allocation);
                        }
                    }

                    ChunkAsyncMesh newMeshResult   = thisChunkData.maybe_async_mesh.get();
                    thisChunkData.maybe_async_mesh = {};

                    const util::RangeAllocation newBrickAllocation =
                        this->brick_range_allocator.allocate(
                            static_cast<u32>(newMeshResult.new_material_bricks.size()));
                    util::assertFatal(
                        newMeshResult.new_material_bricks.size()
                                == newMeshResult.new_shadow_bricks.size()
                            && newMeshResult.new_shadow_bricks.size()
                                   == newMeshResult.new_parent_bricks.size()
                            && newMeshResult.new_parent_bricks.size()
                                   == newMeshResult.new_primary_ray_bricks.size(),
                        "Dont mess this up {} {} {} {}",
                        newMeshResult.new_material_bricks.size(),
                        newMeshResult.new_shadow_bricks.size(),
                        newMeshResult.new_parent_bricks.size(),
                        newMeshResult.new_primary_ray_bricks.size());

                    thisChunkData.active_brick_range_allocation = newBrickAllocation;

                    const PerChunkGpuData& oldGpuData = this->gpu_chunk_data.read(chunkId);

                    this->gpu_chunk_data.write(
                        chunkId,
                        PerChunkGpuData {
                            .world_offset_x {oldGpuData.world_offset_x},
                            .world_offset_y {oldGpuData.world_offset_y},
                            .world_offset_z {oldGpuData.world_offset_z},
                            .brick_allocation_offset {newBrickAllocation.offset},
                            .data {newMeshResult.new_brick_map}});

                    stager.enqueueTransfer(
                        this->per_brick_chunk_parent_info,
                        newBrickAllocation.offset,
                        {newMeshResult.new_parent_bricks});

                    this->material_bricks.write(
                        newBrickAllocation.offset, newMeshResult.new_material_bricks);
                    this->shadow_bricks.write(
                        newBrickAllocation.offset, newMeshResult.new_shadow_bricks);

                    std::copy( // NOLINT
                        newMeshResult.new_primary_ray_bricks.cbegin(),
                        newMeshResult.new_primary_ray_bricks.cend(),
                        this->primary_ray_bricks.begin() + newBrickAllocation.offset);

                    std::array<util::RangeAllocation, 6> allocations {};

                    for (auto [thisAllocation, faces] :
                         std::views::zip(allocations, newMeshResult.new_greedy_faces))
                    {
                        thisAllocation =
                            this->voxel_face_allocator.allocate(static_cast<u32>(faces.size()));

                        if (!faces.empty())
                        {
                            stager.enqueueTransfer(
                                this->voxel_faces,
                                thisAllocation.offset,
                                {faces.data(), faces.size()});
                        }
                    }

                    thisChunkData.active_draw_allocations = allocations;
                }

                bool isChunkInFrustum = true;

                if (thisChunkData.active_draw_allocations.has_value() && isChunkInFrustum)
                {
                    const glm::vec3 chunkCenterPosition =
                        chunkPosition + (VoxelsPerChunkEdge / 2.0f);

                    u32 normal = 0;

                    for (util::RangeAllocation a : *thisChunkData.active_draw_allocations)
                    {
                        const u32 numberOfFaces = this->voxel_face_allocator.getSizeOfAllocation(a);

                        const glm::vec3 normalVector = static_cast<glm::vec3>(
                            getDirFromDirection(static_cast<VoxelFaceDirection>(normal)));
                        const glm::vec3 toChunkVector =
                            glm::normalize(chunkCenterPosition - camera.getPosition());
                        const glm::vec3 forwardVector = camera.getForwardVector();

                        const glm::i32vec3 chunkCenterInteger =
                            static_cast<glm::i32vec3>(chunkCenterPosition);
                        const glm::i32vec3 cameraCenterInteger =
                            static_cast<glm::i32vec3>(camera.getPosition());

                        const glm::i32vec3 chunkCoordinate =
                            chunkCenterInteger / static_cast<i32>(voxel::VoxelsPerChunkEdge);
                        const glm::i32vec3 cameraCoordinate =
                            cameraCenterInteger / static_cast<i32>(voxel::VoxelsPerChunkEdge);

                        const bool doesChunkShareAxis =
                            glm::any(glm::equal(chunkCoordinate, cameraCoordinate));

                        // TODO: refine bounds on axies
                        if (glm::distance(chunkCenterPosition, camera.getPosition())
                                > VoxelsPerChunkEdge
                            && (glm::dot(forwardVector, toChunkVector) < -std::cos(std::min({
                                    this->game->getFovXRadians(),
                                    this->game->getFovYRadians(),
                                }))

                                || glm::dot(forwardVector, toChunkVector) < 0.0f

                                || (glm::dot(toChunkVector, normalVector) > 0.0f
                                    && !doesChunkShareAxis)))
                        {}
                        else
                        {
                            indirectCommands.push_back(vk::DrawIndirectCommand {
                                .vertexCount {numberOfFaces * 6},
                                .instanceCount {1},
                                .firstVertex {a.offset * 6},
                                .firstInstance {callNumber},
                            });

                            indirectPayload.push_back(ChunkDrawIndirectInstancePayload {
                                .normal {normal}, .chunk_id {chunkId}});

                            callNumber += 1;
                            numberOfTotalFaces += numberOfFaces;
                        }
                        normal += 1;
                    }
                }
            });
        profilerTaskGenerator.stamp("Cull and Spawn Meshes");

        // Update Debug Menu
        ::numberOfChunksAllocated.store(this->chunk_id_allocator.getNumberAllocated());
        ::numberOfChunksPossible.store(MaxChunks);

        const auto [bricksAllocated, bricksPossible] = this->brick_range_allocator.getStorageInfo();
        ::numberOfBricksAllocated.store(bricksAllocated);
        ::numberOfBricksPossible.store(bricksPossible);

        // this is just a data race but idc
        const GlobalVoxelData* globalVoxelData =
            this->global_voxel_data.getGpuDataNonCoherent().data();
        ::numberOfFacesVisible.store(globalVoxelData->readback_number_of_visible_faces);
        const auto [facesAllocated, facesPossible] = this->voxel_face_allocator.getStorageInfo();
        ::numberOfFacesOnGpu.store(numberOfTotalFaces);
        ::numberOfFacesAllocated.store(facesAllocated);
        ::numberOfFacesPossible.store(facesPossible);

        this->raytraced_lights.flushViaStager(stager);
        this->gpu_chunk_data.flushViaStager(stager);
        this->material_bricks.flushViaStager(stager);
        this->shadow_bricks.flushViaStager(stager);

        if (!indirectCommands.empty())
        {
            stager.enqueueTransfer(
                this->indirect_commands,
                0,
                std::span<const vk::DrawIndirectCommand> {indirectCommands});
        }

        if (!indirectPayload.empty())
        {
            stager.enqueueTransfer(
                this->indirect_payload,
                0,
                std::span<const ChunkDrawIndirectInstancePayload> {indirectPayload});
        }

        profilerTaskGenerator.stamp("Do gpu writes");

        game::FrameGenerator::RecordObject preFrameUpdate = game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::PreFrameUpdate},
            .pipeline {nullptr},
            .descriptors {},
            .record_func {
                [this, indirectCommands, indirectPayload](
                    vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                {
                    GlobalVoxelData data {
                        .number_of_visible_faces {},
                        .number_of_calculating_draws_x {},
                        .number_of_calculating_draws_y {},
                        .number_of_calculating_draws_z {},
                        .number_of_lights {
                            this->raytraced_light_allocator.getUpperBoundOnAllocatedElements()},
                        .readback_number_of_visible_faces {},
                    };

                    commandBuffer.updateBuffer(
                        *this->global_voxel_data, 0, sizeof(GlobalVoxelData) - 4, &data);

                    commandBuffer.fillBuffer(*this->visible_face_id_map, 0, vk::WholeSize, ~0U);
                }}};

        game::FrameGenerator::RecordObject chunkDraw = game::FrameGenerator::RecordObject {
            .transform {game::Transform {}},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelRenderer},
            .pipeline {this->voxel_chunk_render_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->voxel_chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[this, size = indirectCommands.size()](
                              vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, u32 id)
                          {
                              commandBuffer.bindVertexBuffers(0, {*this->indirect_payload}, {0});

                              commandBuffer.pushConstants(
                                  layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(u32), &id);

                              commandBuffer.drawIndirect(
                                  *this->indirect_commands, 0, static_cast<u32>(size), 16);
                          }}};

        game::FrameGenerator::RecordObject visibilityDraw = game::FrameGenerator::RecordObject {
            .transform {game::Transform {}},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelVisibilityDetection},
            .pipeline {this->voxel_visibility_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->voxel_chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[this](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              vk::Extent2D fbSize =
                                  this->game->getRenderer()->getWindow()->getFramebufferSize();

                              commandBuffer.dispatch(
                                  util::divideEuclidean(fbSize.width, 32u) + 1,
                                  util::divideEuclidean(fbSize.height, 32u) + 1,
                                  1);
                          }}};

        game::FrameGenerator::RecordObject colorCalculation = game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelColorCalculation},
            .pipeline {this->voxel_color_calculation_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->voxel_chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[this](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              commandBuffer.dispatchIndirect(*this->global_voxel_data, 4);
                          }}};

        game::FrameGenerator::RecordObject colorTransfer = game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelColorTransfer},
            .pipeline {this->voxel_color_transfer_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->voxel_chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              commandBuffer.draw(3, 1, 0, 0);
                          }}};

        return {preFrameUpdate, chunkDraw, visibilityDraw, colorCalculation, colorTransfer};
    }

    boost::dynamic_bitset<u64> ChunkRenderManager::readShadow(
        const Chunk& chunk, std::span<const ChunkLocalPosition> positions)
    {
        const PerChunkGpuData& chunkGpuData =
            this->gpu_chunk_data.read(this->chunk_id_allocator.getValueOfHandle(chunk));

        boost::dynamic_bitset<u64> output {};
        output.resize(positions.size());

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            const ChunkLocalPosition& p = positions[i];

            const auto [bC, bP] = splitChunkLocalPosition(p);

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            const u16 maybeLocalOffset = chunkGpuData.data.data[bC.x][bC.y][bC.z];

            if (maybeLocalOffset != decltype(chunkGpuData.data)::NullOffset)
            {
                const u32 globalBrickOffset =
                    chunkGpuData.brick_allocation_offset + maybeLocalOffset;

                const ShadowBrick& shadowBrick = this->shadow_bricks.read(globalBrickOffset);

                if (shadowBrick.read(bP))
                {
                    output.set(i, true);
                }
            }
        }

        return output;
    }

} // namespace voxel