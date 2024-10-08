#include "chunk_manager.hpp"
#include "brick_map.hpp"
#include "brick_pointer.hpp"
#include "brick_pointer_allocator.hpp"
#include "chunk.hpp"
#include "constants.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "greedy_voxel_face.hpp"
#include "shaders/shaders.hpp"
#include "util/index_allocator.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "voxel/constants.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/visibility_brick.hpp"
#include "voxel_face_direction.hpp"
#include <cstddef>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <memory>
#include <optional>
#include <source_location>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{
    static constexpr u32 MaxChunks          = 65536;
    static constexpr u32 DirectionsPerChunk = 6;
    static constexpr u32 MaxBricks          = 1048576;

    ChunkManager::ChunkManager(const game::Game* game)
        : renderer {game->getRenderer()}
        , chunk_id_allocator {MaxChunks}
        , chunk_data {MaxChunks, InternalChunkData {}}
        , brick_maps(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              MaxChunks)
        , indirect_payload(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eVertexBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk))
        , indirect_commands(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk))
        , brick_allocator(MaxBricks)
        , brick_parent_info(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks))
        , material_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks))
        , visibility_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks))
        , material_buffer {voxel::generateVoxelMaterialBuffer(this->renderer)}
        , voxel_face_allocator {16 * 1024 * 1024, MaxChunks * 6}
        , voxel_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(16 * 1024 * 1024))
        , descriptor_set_layout {this->renderer->getAllocator()->cacheDescriptorSetLayout(
              gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {.bindings {{
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
                  vk::DescriptorSetLayoutBinding {
                      .binding {3},
                      .descriptorType {vk::DescriptorType::eStorageBuffer},
                      .descriptorCount {1},
                      .stageFlags {vk::ShaderStageFlagBits::eVertex},
                      .pImmutableSamplers {nullptr},
                  },
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
              }}})}
        , chunk_renderer_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {this->renderer->getAllocator()
                                       ->cacheShaderModule(shaders::load(
                                           "voxel_chunk.vert"))},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {this->renderer->getAllocator()
                                       ->cacheShaderModule(shaders::load(
                                           "voxel_chunk.frag"))},
                          .entry_point {"main"}},
                  }},
                  .vertex_attributes {{
                      vk::VertexInputAttributeDescription {
                          .location {0},
                          .binding {0},
                          .format {vk::Format::eR32G32B32A32Sfloat},
                          .offset {offsetof(
                              ChunkDrawIndirectInstancePayload, position)}},
                      vk::VertexInputAttributeDescription {
                          .location {1},
                          .binding {0},
                          .format {vk::Format::eR32Uint},
                          .offset {offsetof(
                              ChunkDrawIndirectInstancePayload, normal)}},
                      vk::VertexInputAttributeDescription {
                          .location {2},
                          .binding {0},
                          .format {vk::Format::eR32Uint},
                          .offset {offsetof(
                              ChunkDrawIndirectInstancePayload, chunk_id)}},
                  }},
                  .vertex_bindings {{vk::VertexInputBindingDescription {
                      .binding {0},
                      .stride {sizeof(ChunkDrawIndirectInstancePayload)},
                      .inputRate {vk::VertexInputRate::eInstance}}}},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .discard_enable {false},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eCounterClockwise},
                  .depth_test_enable {true},
                  .depth_write_enable {true},
                  .depth_compare_op {vk::CompareOp::eLess},
                  .color_format {gfx::Renderer::ColorFormat.format},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {vk::ShaderStageFlagBits::eVertex},
                              .offset {0},
                              .size {sizeof(u32)},
                          }}})},
              })}
        , chunk_descriptor_set {this->renderer->getAllocator()
                                    ->allocateDescriptorSet(
                                        **this->descriptor_set_layout)}
        , global_descriptor_set {game->getGlobalInfoDescriptorSet()}
    {
        const vk::DescriptorBufferInfo brickMapBufferInfo {
            .buffer {*this->brick_maps}, .offset {0}, .range {vk::WholeSize}};

        const vk::DescriptorBufferInfo materialBrickInfo {
            .buffer {*this->material_bricks},
            .offset {0},
            .range {vk::WholeSize}};

        const vk::DescriptorBufferInfo visibilityBrickInfo {
            .buffer {*this->visibility_bricks},
            .offset {0},
            .range {vk::WholeSize}};

        const vk::DescriptorBufferInfo voxelFacesBufferInfo {
            .buffer {*this->voxel_faces}, .offset {0}, .range {vk::WholeSize}};

        const vk::DescriptorBufferInfo materialBufferInfo {
            .buffer {*this->material_buffer},
            .offset {0},
            .range {vk::WholeSize}};

        this->renderer->getDevice()->getDevice().updateDescriptorSets(
            {
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->chunk_descriptor_set},
                    .dstBinding {0},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eStorageBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&brickMapBufferInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->chunk_descriptor_set},
                    .dstBinding {1},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eStorageBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&materialBrickInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->chunk_descriptor_set},
                    .dstBinding {2},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eStorageBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&visibilityBrickInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->chunk_descriptor_set},
                    .dstBinding {3},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eStorageBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&voxelFacesBufferInfo},
                    .pTexelBufferView {nullptr},
                },
                vk::WriteDescriptorSet {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->chunk_descriptor_set},
                    .dstBinding {4},
                    .dstArrayElement {0},
                    .descriptorCount {1},
                    .descriptorType {vk::DescriptorType::eStorageBuffer},
                    .pImageInfo {nullptr},
                    .pBufferInfo {&materialBufferInfo},
                    .pTexelBufferView {nullptr},
                },
            },
            {});
        util::logTrace(
            "allocated {} bytes", gfx::vulkan::bufferBytesAllocated.load());
    }

    ChunkManager::~ChunkManager()
    {
        while (!this->global_chunks.empty())
        {
            auto node =
                this->global_chunks.extract(this->global_chunks.begin());

            this->deallocateChunk(std::move(node.mapped()));
        }
    }

    std::vector<game::FrameGenerator::RecordObject>
    // NOLINTNEXTLINE
    ChunkManager::makeRecordObject(const game::Game* game, game::Camera c)
    {
        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        u32 callNumber = 0;

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&, this](u32 chunkId)
            {
                InternalChunkData& thisChunkData = this->chunk_data[chunkId];

                if (thisChunkData.needs_remesh)
                {
                    if (thisChunkData.face_data.has_value())
                    {
                        for (util::RangeAllocation a : *thisChunkData.face_data)
                        {
                            this->voxel_face_allocator.free(a);
                        }
                    }
                    thisChunkData.face_data    = this->meshChunkGreedy(chunkId);
                    thisChunkData.needs_remesh = false;
                }

                // TODO: visibility tests cpu-side culling
                bool isChunkInFrustum = false;

                isChunkInFrustum |=
                    (glm::distance(
                         c.getPosition(), thisChunkData.position.xyz())
                     < ChunkEdgeLengthVoxels * 2);

                isChunkInFrustum |=
                    (glm::all(glm::lessThan(
                         c.getPosition() - thisChunkData.position.xyz(),
                         glm::vec3 {ChunkEdgeLengthVoxels}))
                     && glm::all(glm::greaterThan(
                         c.getPosition() - thisChunkData.position.xyz(),
                         glm::vec3 {0})));

                for (auto x :
                     {thisChunkData.position.x,
                      thisChunkData.position.x + ChunkEdgeLengthVoxels})
                {
                    for (auto y :
                         {thisChunkData.position.y,
                          thisChunkData.position.y + ChunkEdgeLengthVoxels})
                    {
                        for (auto z :
                             {thisChunkData.position.z,
                              thisChunkData.position.z + ChunkEdgeLengthVoxels})
                        {
                            glm::vec4 cornerPos {x, y, z, 1.0};

                            auto res =
                                c.getPerspectiveMatrix(
                                    *game,
                                    game::Transform {.translation {cornerPos}})
                                * glm::vec4 {0.0, 0.0, 0.0, 1.0};

                            if (glm::all(glm::lessThan(
                                    res.xyz() / res.w, glm::vec3 {1.0}))
                                && glm::all(glm::greaterThan(
                                    res.xyz() / res.w, glm::vec3 {-1.0})))
                            {
                                isChunkInFrustum = true;
                            }
                        }
                    }
                }

                if (thisChunkData.face_data.has_value() && isChunkInFrustum)
                {
                    u32 normal = 0;

                    for (util::RangeAllocation a : *thisChunkData.face_data)
                    {
                        indirectCommands.push_back(vk::DrawIndirectCommand {
                            .vertexCount {
                                this->voxel_face_allocator.getSizeOfAllocation(
                                    a)
                                * 6},
                            .instanceCount {1},
                            .firstVertex {a.offset * 6},
                            .firstInstance {callNumber},
                        });

                        indirectPayload.push_back(
                            ChunkDrawIndirectInstancePayload {
                                .position {thisChunkData.position},
                                .normal {normal},
                                .chunk_id {chunkId}});

                        callNumber += 1;
                        normal += 1;
                    }
                }
            });

        std::span<vk::DrawIndirectCommand> gpuIndirectCommands =
            this->indirect_commands.getDataNonCoherent();
        std::span<ChunkDrawIndirectInstancePayload> gpuIndirectPayload =
            this->indirect_payload.getDataNonCoherent();

        const gfx::vulkan::FlushData wholeFlush {
            .offset_elements {0}, .size_elements {indirectCommands.size()}};

        this->indirect_commands.flush({&wholeFlush, 1});
        this->indirect_payload.flush({&wholeFlush, 1});

        this->brick_maps.flush();
        this->brick_parent_info.flush();
        this->material_bricks.flush();
        this->visibility_bricks.flush();

        game::FrameGenerator::RecordObject update =
            game::FrameGenerator::RecordObject {
                .transform {},
                .render_pass {
                    game::FrameGenerator::DynamicRenderingPass::PreFrameUpdate},
                .pipeline {nullptr},
                .descriptors {},
                .record_func {[this, indirectCommands, indirectPayload](
                                  vk::CommandBuffer commandBuffer,
                                  vk::PipelineLayout,
                                  u32)
                              {
                                  commandBuffer.updateBuffer(
                                      *this->indirect_commands,
                                      0,
                                      std::span {indirectCommands}.size_bytes(),
                                      indirectCommands.data());

                                  commandBuffer.updateBuffer(
                                      *this->indirect_payload,
                                      0,
                                      std::span {indirectPayload}.size_bytes(),
                                      indirectPayload.data());
                              }}};

        game::FrameGenerator::RecordObject chunkDraw =
            game::FrameGenerator::RecordObject {
                .transform {game::Transform {}},
                .render_pass {
                    game::FrameGenerator::DynamicRenderingPass::SimpleColor},
                .pipeline {this->chunk_renderer_pipeline},
                .descriptors {
                    {this->global_descriptor_set,
                     this->chunk_descriptor_set,
                     nullptr,
                     nullptr}},
                .record_func {[this, indirectCommands, indirectPayload](
                                  vk::CommandBuffer  commandBuffer,
                                  vk::PipelineLayout layout,
                                  u32                id)
                              {
                                  commandBuffer.bindVertexBuffers(
                                      0, {*this->indirect_payload}, {0});

                                  commandBuffer.pushConstants(
                                      layout,
                                      vk::ShaderStageFlagBits::eVertex,
                                      0,
                                      sizeof(u32),
                                      &id);

                                  commandBuffer.drawIndirect(
                                      *this->indirect_commands,
                                      0,
                                      static_cast<u32>(indirectCommands.size()),
                                      16);
                              }}};

        return {chunkDraw, update};
    }

    void ChunkManager::writeVoxel(glm::i32vec3 p, Voxel v)
    {
        glm::i32vec3 coord = glm::i32vec3 {
            util::divideEuclidean(p.x, 64),
            util::divideEuclidean(p.y, 64),
            util::divideEuclidean(p.z, 64),
        };

        ChunkLocalPosition pos {glm::u8vec3 {
            static_cast<u8>(util::moduloEuclidean(p.x, 64)),
            static_cast<u8>(util::moduloEuclidean(p.y, 64)),
            static_cast<u8>(util::moduloEuclidean(p.z, 64)),
        }};

        auto it = this->global_chunks.find(coord);

        if (it == this->global_chunks.end())
        {
            it = this->global_chunks
                     .insert({coord, this->allocateChunk(coord * 64)})
                     .first;
        }

        this->writeVoxelToChunk(it->second, pos, v);
    }

    Chunk ChunkManager::allocateChunk(glm::vec3 position)
    {
        const std::expected<u32, util::IndexAllocator::OutOfBlocks>
            maybeThisChunkId = this->chunk_id_allocator.allocate();

        if (!maybeThisChunkId.has_value())
        {
            util::panic("Failed to allocate new chunk!");
        }

        const u32 thisChunkId = *maybeThisChunkId;

        this->chunk_data[thisChunkId] = InternalChunkData {
            .position {glm::vec4 {position.x, position.y, position.z, 0.0}},
            .face_data {std::nullopt},
            .needs_remesh {true}};

        const BrickMap emptyBrickMap {};
        this->brick_maps.write(thisChunkId, {&emptyBrickMap, 1});

        return Chunk {thisChunkId};
    }

    void ChunkManager::deallocateChunk(Chunk toFree)
    {
        this->chunk_id_allocator.free(toFree.id);

        // TODO: iterate over brick maps and free bricks
        BrickMap& thisChunkBrickMap = this->brick_maps.modify(toFree.id);

        for (auto& yz : thisChunkBrickMap.data)
        {
            for (auto& z : yz)
            {
                for (MaybeBrickPointer& ptr : z)
                {
                    if (!ptr.isNull())
                    {
                        this->brick_allocator.free(BrickPointer {ptr.pointer});
                    }
                }
            }
        }

        std::optional<std::array<util::RangeAllocation, 6>>& maybeFacesToFree =
            this->chunk_data[toFree.id].face_data;

        if (maybeFacesToFree.has_value())
        {
            for (util::RangeAllocation allocation : *maybeFacesToFree)
            {
                this->voxel_face_allocator.free(allocation);
            }
        }

        toFree.id = Chunk::NullChunk;
    }

    void ChunkManager::writeVoxelToChunk(
        const Chunk& c, ChunkLocalPosition p, Voxel v, std::source_location loc)
    {
        this->chunk_data[c.id].needs_remesh = true;

        util::assertFatal<u8, u8, u8>(
            p.x < ChunkEdgeLengthVoxels && p.y < ChunkEdgeLengthVoxels
                && p.z < ChunkEdgeLengthVoxels,
            "ChunkManager::writeVoxelToChunk position [{}, {}, {}] out of "
            "bounds",
            static_cast<u8>(p.x),
            static_cast<u8>(p.y),
            static_cast<u8>(p.z),
            loc);

        const auto [bC, bP] = splitChunkLocalPosition(p);

        MaybeBrickPointer maybeBrickPointer =
            this->brick_maps.read(c.id, 1)[0].data[bC.x][bC.y][bC.z]; // NOLINT

        if (maybeBrickPointer.isNull())
        {
            const BrickPointer newBrickPointer =
                this->brick_allocator.allocate();

            maybeBrickPointer = newBrickPointer;

            this->brick_parent_info.modify(maybeBrickPointer.pointer) =
                BrickParentInformation {
                    .parent_chunk {c.id},
                    .position_in_parent_chunk {bC.getLinearPositionInChunk()}};
            this->material_bricks.modify(maybeBrickPointer.pointer)
                .fill(Voxel::NullAirEmpty);
            this->visibility_bricks.modify(maybeBrickPointer.pointer)
                .fill(false);

            // NOLINTNEXTLINE
            this->brick_maps.modify(c.id).data[bC.x][bC.y][bC.z] =
                maybeBrickPointer;
        }

        // NOLINTNEXTLINE
        this->material_bricks.modify(maybeBrickPointer.pointer)
            .data[bP.x][bP.y][bP.z] = v;

        if (v == Voxel::NullAirEmpty)
        {
            this->visibility_bricks.modify(maybeBrickPointer.pointer)
                .write(bP, false);
        }
        else
        {
            this->visibility_bricks.modify(maybeBrickPointer.pointer)
                .write(bP, true);
        }
    }

    // std::array<util::RangeAllocation, 6>
    // ChunkManager::meshChunkNormal(u32 chunkId) // NOLINT
    // {
    //     std::array<util::RangeAllocation, 6> outAllocations {};

    //     u32 normalDirection = 0;
    //     for (util::RangeAllocation& a : outAllocations)
    //     {
    //         std::vector<GreedyVoxelFace> faces {};

    //         const BrickMap& thisBrickMap = this->brick_maps.read(chunkId,
    //         1)[0];

    //         thisBrickMap.iterateOverPointers(
    //             // NOLINTNEXTLINE
    //             [&](BrickCoordinate bC, MaybeBrickPointer ptr)
    //             {
    //                 if (!ptr.isNull())
    //                 {
    //                     const VisibilityBrick& thisBrick =
    //                         this->visibility_bricks.read(ptr.pointer, 1)[0];

    //                     thisBrick.iterateOverVoxels(
    //                         [&](BrickLocalPosition bP, bool isFilled)
    //                         {
    //                             VoxelFaceDirection dir =
    //                                 static_cast<VoxelFaceDirection>(
    //                                     normalDirection);

    //                             ChunkLocalPosition pos =
    //                                 assembleChunkLocalPosition(bC, bP);

    //                             std::optional<ChunkLocalPosition> adjPos =
    //                                 tryMakeChunkLocalPosition(
    //                                     getDirFromDirection(dir)
    //                                     + static_cast<glm::i8vec3>(pos));

    //                             auto emit = [&]
    //                             {
    //                                 faces.push_back(GreedyVoxelFace {
    //                                     .x {pos.x},
    //                                     .y {pos.y},
    //                                     .z {pos.z},
    //                                     .width {1},
    //                                     .height {1},
    //                                     .pad {0}});
    //                             };

    //                             if (isFilled)
    //                             {
    //                                 if (!adjPos.has_value())
    //                                 {
    //                                     emit();
    //                                 }
    //                                 else
    //                                 {
    //                                     const auto [adjBC, adjBP] =
    //                                         splitChunkLocalPosition(*adjPos);

    //                                     if (adjBC == bC)
    //                                     {
    //                                         if (!thisBrick.read(adjBP))
    //                                         {
    //                                             emit();
    //                                         }
    //                                     }
    //                                     else
    //                                     {
    //                                         MaybeBrickPointer adjBrickPointer
    //                                         =
    //                                             thisBrickMap
    //                                                 .data[adjBC.x][adjBC.y]
    //                                                      [adjBC.z];

    //                                         if (!adjBrickPointer.isNull())
    //                                         {
    //                                             if (!this->visibility_bricks
    //                                                      .read(
    //                                                          adjBrickPointer
    //                                                              .pointer,
    //                                                          1)[0]
    //                                                      .read(adjBP))
    //                                             {
    //                                                 emit();
    //                                             }
    //                                         }
    //                                         else
    //                                         {
    //                                             emit();
    //                                         }
    //                                     }
    //                                 }
    //                             }
    //                         });
    //                 }
    //             });

    //         a = this->voxel_face_allocator.allocate(
    //             static_cast<u32>(faces.size()));

    //         std::copy(
    //             faces.cbegin(),
    //             faces.cend(),
    //             this->voxel_faces.getDataNonCoherent().data() + a.offset);
    //         const gfx::vulkan::FlushData flush {
    //             .offset_elements {a.offset},
    //             .size_elements {faces.size()},
    //         };
    //         this->voxel_faces.flush({&flush, 1});

    //         normalDirection += 1;
    //     }

    //     return outAllocations;
    // }

    std::unique_ptr<ChunkManager::DenseBitChunk>
    ChunkManager::makeDenseBitChunk(u32 chunkId)
    {
        std::unique_ptr<ChunkManager::DenseBitChunk> out =
            std::make_unique<DenseBitChunk>();

        const BrickMap& thisBrickMap = this->brick_maps.read(chunkId, 1)[0];

        thisBrickMap.iterateOverPointers(
            // NOLINTNEXTLINE
            [&](BrickCoordinate bC, MaybeBrickPointer ptr)
            {
                if (!ptr.isNull())
                {
                    const VisibilityBrick& thisBrick =
                        this->visibility_bricks.read(ptr.pointer, 1)[0];

                    thisBrick.iterateOverVoxels(
                        [&](BrickLocalPosition bP, bool isFilled)
                        {
                            ChunkLocalPosition pos =
                                assembleChunkLocalPosition(bC, bP);

                            if (isFilled)
                            {
                                // NOLINTNEXTLINE
                                out->data[pos.x][pos.y] |= (1ULL << pos.z);
                            }
                        });
                }
            });

        return out;
    }

    std::array<util::RangeAllocation, 6>
    ChunkManager::meshChunkGreedy(u32 chunkId) // NOLINT
    {
        std::unique_ptr<DenseBitChunk> trueDenseChunk =
            this->makeDenseBitChunk(chunkId);

        std::array<util::RangeAllocation, 6> outAllocations {};

        u32 normalId = 0;
        for (util::RangeAllocation& a : outAllocations)
        {
            std::unique_ptr<DenseBitChunk> workingChunk =
                std::make_unique<DenseBitChunk>(*trueDenseChunk);

            VoxelFaceDirection dir = static_cast<VoxelFaceDirection>(normalId);
            const auto [widthAxis, heightAxis, ascensionAxis] =
                getDrivingAxes(dir);
            glm::i8vec3 normal = getDirFromDirection(dir);

            std::vector<GreedyVoxelFace> faces {};

            for (i8 ascend = 0; ascend < 64; ++ascend)
            {
                for (i8 height = 0; height < 64; ++height)
                {
                    for (i8 width = 0; width < 64; ++width)
                    {
                        glm::i8vec3 thisRoot = ascensionAxis * ascend
                                             + heightAxis * height
                                             + widthAxis * width;

                        if (!(workingChunk->isOccupied(thisRoot)))
                        {
                            continue;
                        }

                        if (workingChunk->isOccupied(thisRoot + normal))
                        {
                            continue;
                        }

                        i8 widthFaces = 0;

                        while (DenseBitChunk::isPositionInBounds(
                                   thisRoot + (widthFaces * widthAxis))
                               && workingChunk->isOccupied(
                                   thisRoot + (widthFaces * widthAxis)))
                        {
                            if (DenseBitChunk::isPositionInBounds(
                                    normal + thisRoot
                                    + (widthFaces * widthAxis))
                                && workingChunk->isOccupied(
                                    normal + thisRoot
                                    + (widthFaces * widthAxis)))
                            {
                                break;
                            }
                            else
                            {
                                widthFaces += 1;
                            }
                        }

                        util::assertFatal(
                            widthFaces > 0 && widthFaces <= 64,
                            " {} ",
                            widthFaces);

                        faces.push_back(GreedyVoxelFace {
                            .x {static_cast<u32>(thisRoot.x)},
                            .y {static_cast<u32>(thisRoot.y)},
                            .z {static_cast<u32>(thisRoot.z)},
                            .width {static_cast<u32>(widthFaces - 1)},
                            .height {0},
                            .pad {0}});

                        width += widthFaces - 1; // NOLINT
                    }
                }
            }

            a = this->voxel_face_allocator.allocate(
                static_cast<u32>(faces.size()));

            std::copy(
                faces.cbegin(),
                faces.cend(),
                this->voxel_faces.getDataNonCoherent().data() + a.offset);
            const gfx::vulkan::FlushData flush {
                .offset_elements {a.offset},
                .size_elements {faces.size()},
            };
            this->voxel_faces.flush({&flush, 1});

            normalId += 1;
        }

        return outAllocations;
    }

} // namespace voxel
