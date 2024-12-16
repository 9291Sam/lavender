#include "chunk_render_manager.hpp"
#include "game/game.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "structures.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "util/static_filesystem.hpp"
#include "voxel/material_manager.hpp"
#include <memory>
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

            [[nodiscard]] bool
            isEntireRangeOccupied(glm::i8vec3 base, glm::i8vec3 step, i8 range) const // NOLINT
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
                if constexpr (util::isDebugBuild())
                {
                    util::assertFatal(
                        p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0 && p.z < 64,
                        "{} {} {}",
                        p.x,
                        p.y,
                        p.z);
                }

                if (filled)
                {
                    // NOLINTNEXTLINE
                    this->data[static_cast<std::size_t>(p.x)][static_cast<std::size_t>(p.y)] |=
                        (u64 {1} << static_cast<u64>(p.z));
                }
                else
                {
                    // NOLINTNEXTLINE
                    this->data[static_cast<std::size_t>(p.x)][static_cast<std::size_t>(p.y)] &=
                        ~(u64 {1} << static_cast<u64>(p.z));
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

        struct AsyncChunkUpdateResult
        {};

        // std::future<AsyncChunkUpdateResult> doChunkUpdate()
    } // namespace

    static constexpr u32 MaxChunks          = 4096;
    static constexpr u32 DirectionsPerChunk = 6;
    static constexpr u32 MaxBricks          = 131072 * 8;
    static constexpr u32 MaxFaces           = 1048576 * 4;
    static constexpr u32 MaxLights          = 4096;
    static constexpr u32 MaxFaceIdMapNodes  = 1U << 24U;

    ChunkRenderManager::ChunkRenderManager(const game::Game* game)
        : chunk_id_allocator {MaxChunks}
        , gpu_chunk_data(
              game->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxChunks,
              "Gpu Chunk Data Buffer")
        , brick_range_allocator(MaxBricks, MaxBricks * 2)
        , material_bricks(
              game->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Material Bricks")
        , voxel_face_allocator(MaxFaces, MaxChunks * 12)
        , voxel_faces(
              game->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaces),
              "Voxel Faces")
        , indirect_payload(
              game->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Payload")
        , indirect_commands(
              game->getRenderer()->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Commands")
        , materials(generateVoxelMaterialBuffer(game->getRenderer()))
        , voxel_chunk_descriptor_set_layout(
              game->getRenderer()->getAllocator()->cacheDescriptorSetLayout(
                  gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {
                      .bindings {{
                          // PerChunkGpuData
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
                          // MaterialBricks
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
                          // VoxelFaces
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
                          // Materials
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
                      }},
                      .name {"Voxel Descriptor Set Layout"}}))
        , temporary_voxel_chunk_render_pipeline {game->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {game->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("temporary_voxel_render.vert"),
                              "Temporary Voxel Render Vertex Shader")},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {game->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("temporary_voxel_render.frag"),
                              "Temporary Voxel Render Fragment Shader")},
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
                  .color_format {gfx::Renderer::ColorFormat.format},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .blend_enable {false},
                  .layout {game->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->voxel_chunk_descriptor_set_layout}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {vk::ShaderStageFlagBits::eVertex},
                              .offset {0},
                              .size {sizeof(u32)},
                          }},
                          .name {"Temporary Voxel Render Pipeline Layout"}})},
                  .name {"Temporary Voxel Render Pipeline"}})}
        , global_descriptor_set {game->getGlobalInfoDescriptorSet()}
        , voxel_chunk_descriptor_set {game->getRenderer()->getAllocator()->allocateDescriptorSet(
              **this->voxel_chunk_descriptor_set_layout, "Voxel Descriptor Set")}

    {
        const auto bufferInfo = {
            vk::DescriptorBufferInfo {
                .buffer {*this->gpu_chunk_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->material_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->voxel_faces},
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

        game->getRenderer()->getDevice()->getDevice().updateDescriptorSets(
            static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

        this->cpu_chunk_data.resize(MaxChunks);
    }

    ChunkRenderManager::~ChunkRenderManager() = default;

    ChunkRenderManager::Chunk ChunkRenderManager::createChunk(WorldPosition newChunkWorldPosition)
    {
        const u32 newChunkId = this->chunk_id_allocator.allocateOrPanic();

        this->cpu_chunk_data[newChunkId] = CpuChunkData {};
        const PerChunkGpuData newChunkGpuData {
            .world_offset_x {newChunkWorldPosition.x},
            .world_offset_y {newChunkWorldPosition.y},
            .world_offset_z {newChunkWorldPosition.z},
            .brick_allocation_offset {0},
            .data {}};
        this->gpu_chunk_data.write(newChunkId, newChunkGpuData);

        return Chunk {newChunkId};
    }

    void ChunkRenderManager::destroyChunk(Chunk chunk)
    {
        const u32 chunkId = chunk.release();

        CpuChunkData& thisCpuChunkData = this->cpu_chunk_data[chunkId];

        if (std::optional allocation = thisCpuChunkData.active_brick_range_allocation)
        {
            this->brick_range_allocator.free(*allocation);
        }
    }

    void
    ChunkRenderManager::updateChunk(const Chunk& chunk, std::span<ChunkLocalUpdate> chunkUpdates)
    {
        const u32 chunkId = chunk.value;

        std::vector<ChunkLocalUpdate>& updatesQueue = this->cpu_chunk_data[chunkId].updates;

        for (const ChunkLocalUpdate& update : chunkUpdates)
        {
            updatesQueue.push_back(update);
        }
    }

    std::vector<game::FrameGenerator::RecordObject>
    ChunkRenderManager::processUpdatesAndGetDrawObjects(const gfx::vulkan::BufferStager& stager)
    {
        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        u32 callNumber         = 0;
        u32 numberOfTotalFaces = 0;

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&](const u32 chunkId)
            {
                CpuChunkData& thisChunkData = this->cpu_chunk_data[chunkId];

                if (!thisChunkData.updates.empty())
                {
                    const PerChunkGpuData oldGpuData = this->gpu_chunk_data.read(chunkId);

                    const std::size_t oldBricksPerChunk = [&] -> std::size_t
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

                    const std::span<const MaterialBrick> oldBricks = this->material_bricks.read(
                        oldGpuData.brick_allocation_offset, oldBricksPerChunk);

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

                    util::IndexAllocator newChunkOffsetAllocator {
                        BricksPerChunkEdge * BricksPerChunkEdge * BricksPerChunkEdge};
                    ChunkBrickMap              newBrickMap {};
                    std::vector<MaterialBrick> newBricks {};

                    // Propagate old updates
                    oldGpuData.data.iterateOverBricks(
                        [&](BrickCoordinate bC, u16 oldOffset)
                        {
                            if (oldOffset != ChunkBrickMap::NullOffset)
                            {
                                const u16 newOffset =
                                    static_cast<u16>(newChunkOffsetAllocator.allocateOrPanic());

                                newBrickMap.setOffset(bC, newOffset);

                                newBricks.push_back(oldBricks[oldOffset]);
                            }
                        });

                    const std::vector<ChunkLocalUpdate> newUpdates =
                        std::move(thisChunkData.updates);

                    for (const ChunkLocalUpdate& newUpdate : newUpdates)
                    {
                        const ChunkLocalPosition updatePosition = newUpdate.getPosition();
                        const Voxel              updateVoxel    = newUpdate.getVoxel();
                        const ChunkLocalUpdate::ShadowUpdate shadowUpdate =
                            newUpdate.getShadowUpdate();
                        const ChunkLocalUpdate::CameraVisibleUpdate cameraVisibilityUpdate =
                            newUpdate.getCameraVisibility();

                        const auto [coordinate, local] = splitChunkLocalPosition(updatePosition);

                        u16 maybeOffset = newBrickMap.getOffset(coordinate);
                        if (maybeOffset == ChunkBrickMap::NullOffset)
                        {
                            maybeOffset =
                                static_cast<u16>(newChunkOffsetAllocator.allocateOrPanic());

                            newBrickMap.setOffset(coordinate, maybeOffset);
                            newBricks.push_back(MaterialBrick {});
                        }
                        newBricks[maybeOffset].write(local, updateVoxel);
                    }

                    BrickList list {};

                    newBrickMap.iterateOverBricks(
                        [&](BrickCoordinate bC, u16 maybeOffset)
                        {
                            if (maybeOffset != ChunkBrickMap::NullOffset)
                            {
                                const MaterialBrick& materialBrick = newBricks[maybeOffset];
                                BitBrick             bitBrick {};

                                materialBrick.iterateOverVoxels(
                                    [&](BrickLocalPosition bP, Voxel v)
                                    {
                                        if (v != Voxel::NullAirEmpty)
                                        {
                                            bitBrick.write(bP, true);
                                        }
                                    });

                                list.data.push_back({bC, bitBrick});
                            }
                        });

                    const std::array<std::vector<GreedyVoxelFace>, 6> newGreedyFaces =
                        meshChunkGreedy(list.formDenseBitChunk());

                    const util::RangeAllocation newBrickAllocation =
                        this->brick_range_allocator.allocate(static_cast<u32>(newBricks.size()));

                    thisChunkData.active_brick_range_allocation = newBrickAllocation;

                    this->gpu_chunk_data.write(
                        chunkId,
                        PerChunkGpuData {
                            .world_offset_x {oldGpuData.world_offset_x},
                            .world_offset_y {oldGpuData.world_offset_y},
                            .world_offset_z {oldGpuData.world_offset_z},
                            .brick_allocation_offset {newBrickAllocation.offset},
                            .data {newBrickMap}});

                    this->material_bricks.write(newBrickAllocation.offset, newBricks);

                    std::array<util::RangeAllocation, 6> allocations {};

                    for (auto [thisAllocation, faces] :
                         std::views::zip(allocations, newGreedyFaces))
                    {
                        thisAllocation =
                            this->voxel_face_allocator.allocate(static_cast<u32>(faces.size()));

                        stager.enqueueTransfer(
                            this->voxel_faces, thisAllocation.offset, {faces.data(), faces.size()});
                    }

                    thisChunkData.active_draw_allocations = allocations;
                }

                if (thisChunkData.active_draw_allocations.has_value())
                {
                    u32 normal = 0;

                    for (util::RangeAllocation a : *thisChunkData.active_draw_allocations)
                    {
                        const u32 numberOfFaces = this->voxel_face_allocator.getSizeOfAllocation(a);

                        indirectCommands.push_back(vk::DrawIndirectCommand {
                            .vertexCount {numberOfFaces * 6},
                            .instanceCount {1},
                            .firstVertex {a.offset * 6},
                            .firstInstance {callNumber},
                        });

                        indirectPayload.push_back(ChunkDrawIndirectInstancePayload {
                            .normal {normal}, .chunk_id {chunkId}});

                        callNumber += 1;
                        normal += 1;
                    }
                }
            });

        this->gpu_chunk_data.flushViaStager(stager);
        this->material_bricks.flushViaStager(stager);

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

        game::FrameGenerator::RecordObject chunkDraw = game::FrameGenerator::RecordObject {
            .transform {game::Transform {}},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {this->temporary_voxel_chunk_render_pipeline},
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

        return {chunkDraw};
    }

} // namespace voxel