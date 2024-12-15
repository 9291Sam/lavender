#include "chunk_render_manager.hpp"
#include "structures.hpp"
#include "util/index_allocator.hpp"
#include <memory>
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

    ChunkRenderManager::~ChunkRenderManager() = default;

    ChunkRenderManager::Chunk ChunkRenderManager::createChunk(WorldPosition newChunkWorldPosition)
    {
        const u32 newChunkId = this->chunk_id_allocator.allocateOrPanic();

        this->cpu_chunk_data[newChunkId] = CpuChunkData {};
        const PerChunkGpuData newChunkGpuData {
            .world_offset_x {static_cast<f32>(newChunkWorldPosition.x)},
            .world_offset_y {static_cast<f32>(newChunkWorldPosition.y)},
            .world_offset_z {static_cast<f32>(newChunkWorldPosition.z)},
            .brick_allocation_offset {0},
            .data {}};
        this->gpu_chunk_data.uploadImmediate(newChunkId, {&newChunkGpuData, 1});

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

        if (thisCpuChunkData.meshing_operation.valid())
        {
            // thisCpuChunkData.meshing_operation.get()
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
    ChunkRenderManager::processUpdatesAndGetDrawObjects()
    {
        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&](const u32 chunkId)
            {
                CpuChunkData& thisChunkData = this->cpu_chunk_data[chunkId];

                if (!thisChunkData.updates.empty())
                {
                    const PerChunkGpuData oldGpuData        = this->gpu_chunk_data.read(chunkId);
                    const std::size_t     oldBricksPerChunk = [&] -> std::size_t
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
                    }

                    // iterate over old
                    // add new
                    // clear dense
                    // repack
                    // create allocations of bricks
                    // do greedy mesh
                    // dump to correct spots

                    // TODO: free old stuff
                }
            });
    }

} // namespace voxel