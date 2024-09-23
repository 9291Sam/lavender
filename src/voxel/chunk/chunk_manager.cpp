

#include "chunk_manager.hpp"
#include "chunk.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "shaders/shaders.hpp"
#include "util/index_allocator.hpp"
#include "util/range_allocator.hpp"
#include "voxel/brick/brick_map.hpp"
#include "voxel/brick/brick_pointer.hpp"
#include "voxel/brick/brick_pointer_allocator.hpp"
#include "voxel/constants.hpp"
#include "voxel/data/greedy_voxel_face.hpp"
#include <cstddef>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel::chunk
{
    static constexpr u32 MaxChunks = 4096;

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
              vk::BufferUsageFlagBits::eVertexBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * 6))
        , indirect_commands(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * 6))
        , brick_allocator(MaxChunks * 8 * 8 * 2)
        , material_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * 8 * 8 * 4))
        , voxel_face_allocator {16 * 1024 * 1024, MaxChunks * 6}
        , voxel_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(16 * 1024 * 1024))
        , descriptor_set_layout {this->renderer->getAllocator()->cacheDescriptorSetLayout(
              gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {.bindings {
                  {vk::DescriptorSetLayoutBinding {
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
                       .stageFlags {vk::ShaderStageFlagBits::eVertex},
                       .pImmutableSamplers {nullptr},
                   }}}})}
        , chunk_renderer_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_chunk.vert"))},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_chunk.frag"))},
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

        const vk::DescriptorBufferInfo voxelFacesBufferInfo {
            .buffer {*this->voxel_faces}, .offset {0}, .range {vk::WholeSize}};

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
                    .pBufferInfo {&voxelFacesBufferInfo},
                    .pTexelBufferView {nullptr},
                },
            },
            {});
        util::logTrace(
            "allocated {} bytes", gfx::vulkan::bufferBytesAllocated.load());
    }

    ChunkManager::~ChunkManager() = default;

    game::FrameGenerator::RecordObject ChunkManager::makeRecordObject()
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
                    thisChunkData.face_data    = this->meshChunk(chunkId);
                    thisChunkData.needs_remesh = false;
                }

                // TODO: visibility tests cpu-side culling

                if (thisChunkData.face_data.has_value())
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

        std::copy(
            indirectCommands.cbegin(),
            indirectCommands.cend(),
            gpuIndirectCommands.begin());

        std::copy(
            indirectPayload.cbegin(),
            indirectPayload.cend(),
            gpuIndirectPayload.begin());

        const gfx::vulkan::FlushData wholeFlush {
            .offset_elements {0}, .size_elements {indirectCommands.size()}};

        this->indirect_commands.flush({&wholeFlush, 1});
        this->indirect_payload.flush({&wholeFlush, 1});

        return game::FrameGenerator::RecordObject {
            .transform {game::Transform {}},
            .render_pass {
                game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {this->chunk_renderer_pipeline},
            .descriptors {
                {this->global_descriptor_set,
                 this->chunk_descriptor_set,
                 nullptr,
                 nullptr}},
            .record_func {
                [this, numberOfIndirectCommands = indirectCommands.size()](
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
                        static_cast<u32>(numberOfIndirectCommands),
                        16);
                }}};
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

        const brick::BrickMap emptyBrickMap {};
        this->brick_maps.write(thisChunkId, {&emptyBrickMap, 1});

        return Chunk {thisChunkId};
    }

    void ChunkManager::deallocateChunk(Chunk&& chunkToMoveFrom)
    {
        Chunk toFree {std::move(chunkToMoveFrom)};

        this->chunk_id_allocator.free(toFree.id);

        // TODO: iterate over brick maps and free bricks
        brick::BrickMap& thisChunkBrickMap = this->brick_maps.modify(toFree.id);

        for (auto& yz : thisChunkBrickMap.data)
        {
            for (auto& z : yz)
            {
                for (brick::MaybeBrickPointer& ptr : z)
                {
                    if (!ptr.isNull())
                    {
                        this->brick_allocator.free(
                            brick::BrickPointer {ptr.pointer});
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
    }

    void ChunkManager::writeVoxelToChunk(
        const Chunk& c, ChunkLocalPosition p, Voxel v)
    {
        this->chunk_data[c.id].needs_remesh = true;

        const auto [bC, bP] = splitChunkLocalPosition(p);

        util::logTrace("inserting at [{}, {}, {}]", p.x, p.y, p.z);

        brick::MaybeBrickPointer maybeBrickPointer =
            this->brick_maps.read(c.id, 1)[0].data[bC.x][bC.y][bC.z];

        if (maybeBrickPointer.isNull())
        {
            const brick::BrickPointer newBrickPointer =
                this->brick_allocator.allocate();

            maybeBrickPointer = newBrickPointer;

            this->material_bricks.modify(maybeBrickPointer.pointer)
                .fill(Voxel::NullAirEmpty);

            this->brick_maps.modify(c.id).data[bC.x][bC.y][bC.z] =
                maybeBrickPointer;
        }

        this->material_bricks.modify(maybeBrickPointer.pointer)
            .data[bP.x][bP.y][bP.z] = v;
    }

    std::array<util::RangeAllocation, 6> ChunkManager::meshChunk(u32 chunkId)
    {
        util::logTrace("starting mesh of chunk {}", chunkId);

        std::array<util::RangeAllocation, 6> outAllocations {};

        u32 normal_direction = 0;
        for (util::RangeAllocation& a : outAllocations)
        {
            std::vector<data::GreedyVoxelFace> faces {};

            this->brick_maps.read(chunkId, 1)[0].iterateOverPointers(
                [&](BrickCoordinate bC, brick::MaybeBrickPointer ptr)
                {
                    if (!ptr.isNull())
                    {
                        this->material_bricks.read(ptr.pointer, 1)[0]
                            .iterateOverVoxels(
                                [&](BrickLocalPosition bP, Voxel v)
                                {
                                    if (v != Voxel::NullAirEmpty)
                                    {
                                        ChunkLocalPosition pos =
                                            assembleChunkLocalPosition(bC, bP);

                                        util::logTrace(
                                            "meshing at [{}, {}, {}]",
                                            pos.x,
                                            pos.y,
                                            pos.z);

                                        faces.push_back(data::GreedyVoxelFace {
                                            .x {pos.x},
                                            .y {pos.y},
                                            .z {pos.z},
                                            .width {1},
                                            .height {1},
                                            .pad {0}});
                                    }
                                });
                    }
                });

            a = this->voxel_face_allocator.allocate(
                static_cast<u32>(faces.size()));

            std::copy(
                faces.cbegin(),
                faces.cend(),
                this->voxel_faces.getDataNonCoherent().data());
            const gfx::vulkan::FlushData flush {
                .offset_elements {a.offset},
                .size_elements {faces.size()},
            };
            this->voxel_faces.flush({&flush, 1});

            normal_direction += 1;
        }

        util::logTrace("ending mesh of chunk {}", chunkId);

        return outAllocations;
    }

} // namespace voxel::chunk
