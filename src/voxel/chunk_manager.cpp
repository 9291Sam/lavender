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
#include "gfx/vulkan/buffer_stager.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/window.hpp"
#include "greedy_voxel_face.hpp"
#include "opacity_brick.hpp"
#include "point_light.hpp"
#include "shaders/shaders.hpp"
#include "util/index_allocator.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "voxel/constants.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/opacity_brick.hpp"
#include "voxel_face_direction.hpp"
#include <bit>
#include <cstddef>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <initializer_list>
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

    static constexpr u32 MaxChunks          = 4096;
    static constexpr u32 DirectionsPerChunk = 6;
    static constexpr u32 MaxBricks          = 131072;
    static constexpr u32 MaxFaces           = 1048576 * 4;

    ChunkManager::ChunkManager(const game::Game* game)
        : renderer {game->getRenderer()}
        , chunk_id_allocator {MaxChunks}
        , cpu_chunk_data {MaxChunks, CpuChunkData {}}
        , gpu_chunk_data(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              MaxChunks)
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
        , opacity_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks))
        , visibility_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks))
        , face_id_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks))
        , voxel_face_allocator {MaxFaces, MaxChunks * 6}
        , voxel_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal
                  | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxFaces))
        , material_buffer {voxel::generateVoxelMaterialBuffer(this->renderer)}
        , number_of_visible_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst
                  | vk::BufferUsageFlagBits::eIndirectBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1)
        , visible_face_data(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer
                  | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaces))
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
                      .stageFlags {
                          vk::ShaderStageFlagBits::eVertex
                          | vk::ShaderStageFlagBits::eFragment
                          | vk::ShaderStageFlagBits::eCompute},
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
                  vk::DescriptorSetLayoutBinding {
                      .binding {11},
                      .descriptorType {vk::DescriptorType::eStorageBuffer},
                      .descriptorCount {1},
                      .stageFlags {
                          vk::ShaderStageFlagBits::eVertex
                          | vk::ShaderStageFlagBits::eFragment
                          | vk::ShaderStageFlagBits::eCompute},
                      .pImmutableSamplers {nullptr},
                  },
              }}})}
        , voxel_render_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_render.vert"))},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_render.frag"))},
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
                  .color_format {vk::Format::eR32Uint},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .blend_enable {false},
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
        , voxel_visibility_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {this->renderer->getAllocator()->cacheShaderModule(
                      shaders::load(
                          "voxel_visibility_detection.comp"))},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {}})},
              })}
        , voxel_color_calculation_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {this->renderer->getAllocator()->cacheShaderModule(
                      shaders::load("voxel_color_calculation.comp"))},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {}})},
              })}
        , voxel_color_transfer_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_color_transfer."
                                                "vert"))},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {
                              this->renderer->getAllocator()->cacheShaderModule(
                                  shaders::load("voxel_color_transfer."
                                                "frag"))},
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
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {}})},
              })}
        , chunk_descriptor_set {this->renderer->getAllocator()
                                    ->allocateDescriptorSet(
                                        **this->descriptor_set_layout)}
        , global_descriptor_set {game->getGlobalInfoDescriptorSet()}
    {
        const auto bufferInfo = {
            vk::DescriptorBufferInfo {
                .buffer {*this->gpu_chunk_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->brick_maps},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->brick_parent_info},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->material_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->opacity_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visibility_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->face_id_bricks},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->voxel_faces},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->material_buffer},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->number_of_visible_faces},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visible_face_data},
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
                .dstSet {this->chunk_descriptor_set},
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

        this->renderer->getDevice()->getDevice().updateDescriptorSets(
            static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

        const std::size_t bytesAllocated =
            gfx::vulkan::bufferBytesAllocated.load();
        const double gibAllocated =
            static_cast<double>(bytesAllocated) / (1024 * 1024 * 1024);

        util::logTrace("allocated {:.3} GiB", gibAllocated);
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
    ChunkManager::makeRecordObject(
        const game::Game*                game,
        const gfx::vulkan::BufferStager& stager,
        game::Camera                     c)
    {
        // util::Timer t {"end make record object"};
        // util::logTrace("starting make record obhject");
        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        u32 callNumber = 0;

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&, this](u32 chunkId)
            {
                CpuChunkData& thisChunkData = this->cpu_chunk_data[chunkId];

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

                // TODO: opacity tests cpu-side culling
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
                      thisChunkData.position.x + ChunkEdgeLengthVoxels / 4.0f,
                      thisChunkData.position.x + ChunkEdgeLengthVoxels / 2.0f,
                      thisChunkData.position.x
                          + ChunkEdgeLengthVoxels / 1.3333f,
                      thisChunkData.position.x + ChunkEdgeLengthVoxels})
                {
                    for (auto y :
                         {thisChunkData.position.y,
                          thisChunkData.position.y
                              + ChunkEdgeLengthVoxels / 4.0f,
                          thisChunkData.position.y
                              + ChunkEdgeLengthVoxels / 2.0f,
                          thisChunkData.position.y
                              + ChunkEdgeLengthVoxels / 1.3333f,
                          thisChunkData.position.y + ChunkEdgeLengthVoxels})
                    {
                        for (auto z :
                             {thisChunkData.position.z,
                              thisChunkData.position.z
                                  + ChunkEdgeLengthVoxels / 4.0f,
                              thisChunkData.position.z
                                  + ChunkEdgeLengthVoxels / 2.0f,
                              thisChunkData.position.z
                                  + ChunkEdgeLengthVoxels / 1.3333f,
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

        this->gpu_chunk_data.flush();
        this->brick_maps.flush();
        this->brick_parent_info.flush();
        this->material_bricks.flush();
        this->opacity_bricks.flush();

        if (!indirectCommands.empty())
        {
            stager.enqueueTransfer(
                this->indirect_commands, 0, std::span {indirectCommands});
        }

        if (!indirectPayload.empty())
        {
            stager.enqueueTransfer(
                this->indirect_payload, 0, std::span {indirectPayload});
        }

        game::FrameGenerator::RecordObject update =
            game::FrameGenerator::RecordObject {
                .transform {},
                .render_pass {
                    game::FrameGenerator::DynamicRenderingPass::PreFrameUpdate},
                .pipeline {nullptr},
                .descriptors {},
                .record_func {
                    [this, indirectCommands, indirectPayload](
                        vk::CommandBuffer commandBuffer,
                        vk::PipelineLayout,
                        u32)
                    {
                        commandBuffer.fillBuffer(
                            *this->visibility_bricks,
                            0,
                            this->visibility_bricks.getDataNonCoherent()
                                .size_bytes(),
                            0);

                        commandBuffer.fillBuffer(
                            *this->number_of_visible_faces,
                            0,
                            this->number_of_visible_faces.getDataNonCoherent()
                                .size_bytes(),
                            0);
                    }}};

        game::FrameGenerator::RecordObject chunkDraw =
            game::FrameGenerator::RecordObject {
                .transform {game::Transform {}},
                .render_pass {
                    game::FrameGenerator::DynamicRenderingPass::VoxelRenderer},
                .pipeline {this->voxel_render_pipeline},
                .descriptors {
                    {this->global_descriptor_set,
                     this->chunk_descriptor_set,
                     nullptr,
                     nullptr}},
                .record_func {[this, size = indirectCommands.size()](
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
                                      static_cast<u32>(size),
                                      16);
                              }}};

        game::FrameGenerator::RecordObject visibilityDraw =
            game::FrameGenerator::RecordObject {
                .transform {game::Transform {}},
                .render_pass {game::FrameGenerator::DynamicRenderingPass::
                                  VoxelVisibilityDetection},
                .pipeline {this->voxel_visibility_pipeline},
                .descriptors {
                    {this->global_descriptor_set,
                     this->chunk_descriptor_set,
                     nullptr,
                     nullptr}},
                .record_func {
                    [this](
                        vk::CommandBuffer commandBuffer,
                        vk::PipelineLayout,
                        u32)
                    {
                        vk::Extent2D fbSize =
                            this->renderer->getWindow()->getFramebufferSize();

                        commandBuffer.dispatch(
                            util::divideEuclidean(fbSize.width, 32u) + 1,
                            util::divideEuclidean(fbSize.height, 32u) + 1,
                            1);
                    }}};

        game::FrameGenerator::RecordObject colorCalculation =
            game::FrameGenerator::RecordObject {
                .transform {},
                .render_pass {game::FrameGenerator::DynamicRenderingPass::
                                  VoxelColorCalculation},
                .pipeline {this->voxel_color_calculation_pipeline},
                .descriptors {
                    {this->global_descriptor_set,
                     this->chunk_descriptor_set,
                     nullptr,
                     nullptr}},
                .record_func {[this](
                                  vk::CommandBuffer commandBuffer,
                                  vk::PipelineLayout,
                                  u32)
                              {
                                  commandBuffer.dispatchIndirect(
                                      *this->number_of_visible_faces, 4);
                              }}};

        game::FrameGenerator::RecordObject colorTransfer =
            game::FrameGenerator::RecordObject {
                .transform {},
                .render_pass {game::FrameGenerator::DynamicRenderingPass::
                                  VoxelColorTransfer},
                .pipeline {this->voxel_color_transfer_pipeline},
                .descriptors {
                    {this->global_descriptor_set,
                     this->chunk_descriptor_set,
                     nullptr,
                     nullptr}},
                .record_func {
                    [](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                    {
                        commandBuffer.draw(3, 1, 0, 0);
                    }}};

        return {
            update, chunkDraw, visibilityDraw, colorCalculation, colorTransfer};
    }

    PointLight ChunkManager::createPointLight(
        glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs)
    {
        // TODO: migrate to a better place
        const ChunkCoordinate coord {glm::i32vec3 {
            util::divideEuclidean(static_cast<i32>(position.x), 64),
            util::divideEuclidean(static_cast<i32>(position.y), 64),
            util::divideEuclidean(static_cast<i32>(position.z), 64),
        }};

        auto maybeChunk = this->global_chunks.find(coord);

        if (maybeChunk == this->global_chunks.cend())
        {
            util::logWarn("Tried to insert light in nonexistant chunk");

            return PointLight {};
        }

        const InternalPointLight internal {
            .position {position.xyzz()},
            .color_and_power {colorAndPower},
            .falloffs {falloffs}};

        CpuChunkData& cpuChunkData =
            this->cpu_chunk_data[maybeChunk->second.id];
        GpuChunkData& gpuChunkData =
            this->gpu_chunk_data.modify(maybeChunk->second.id);

        if (cpuChunkData.lights.contains(internal))
        {
            util::logWarn("duplicate insertion of light");

            return PointLight {};
        }

        cpuChunkData.lights.insert(internal);

        gpuChunkData.number_of_point_lights = cpuChunkData.lights.size();

        util::assertFatal(
            cpuChunkData.lights.size() < 128, "too many lights in one chunk");

        std::copy(
            cpuChunkData.lights.cbegin(),
            cpuChunkData.lights.cend(),
            gpuChunkData.lights.begin());

        return PointLight {maybeChunk->second.id, internal};
    }

    void ChunkManager::destroyPointLight(PointLight toFree)
    {
        if (toFree.isNull())
        {
            util::logWarn("destroy of null PointLight");
            return;
        }

        // TODO: test for chunk aliveness (chunk optional...)
        CpuChunkData& cpuChunkData = this->cpu_chunk_data[toFree.chunk];
        GpuChunkData& gpuChunkData = this->gpu_chunk_data.modify(toFree.chunk);

        auto internalIt = cpuChunkData.lights.find(toFree.data);

        if (internalIt == cpuChunkData.lights.cend())
        {
            util::logWarn("tried to destroy a non existant point light");
            // return;
        }
        else
        {
            cpuChunkData.lights.erase(internalIt);
        }

        // TODO: exact same thing from earlier
        std::copy(
            cpuChunkData.lights.cbegin(),
            cpuChunkData.lights.cend(),
            gpuChunkData.lights.begin());

        gpuChunkData.number_of_point_lights = cpuChunkData.lights.size();

        toFree.chunk = PointLight::NullChunk;
        toFree.data  = {};
    }

    void ChunkManager::writeVoxel(glm::i32vec3 p, Voxel v)
    {
        ChunkCoordinate coord {glm::i32vec3 {
            util::divideEuclidean(p.x, 64),
            util::divideEuclidean(p.y, 64),
            util::divideEuclidean(p.z, 64),
        }};

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

    Chunk ChunkManager::allocateChunk(glm::ivec3 position)
    {
        util::assertFatal(
            position % glm::ivec3 {64, 64, 64} == glm::ivec3 {0, 0, 0},
            "Tried to create a chunk at {}",
            glm::to_string(position));

        const std::expected<u32, util::IndexAllocator::OutOfBlocks>
            maybeThisChunkId = this->chunk_id_allocator.allocate();

        if (!maybeThisChunkId.has_value())
        {
            util::panic("Failed to allocate new chunk!");
        }

        const u32 thisChunkId = *maybeThisChunkId;

        this->cpu_chunk_data[thisChunkId] = CpuChunkData {
            .position {glm::vec4 {position.x, position.y, position.z, 0.0}},
            .face_data {std::nullopt},
            .needs_remesh {true},
            .slice_data {}};

        const BrickMap emptyBrickMap {};
        this->brick_maps.write(thisChunkId, {&emptyBrickMap, 1});

        GpuChunkData gpuChunkData {
            .position {position.xyzz()}, .adjacent_chunks {}};

        // TODO: make better
        std::memset(
            gpuChunkData.adjacent_chunks.data(),
            -1,
            sizeof(gpuChunkData.adjacent_chunks));

        std::initializer_list<i32> relativePositions {-64, 0, 64};

        for (i32 x : relativePositions)
        {
            for (i32 y : relativePositions)
            {
                for (i32 z : relativePositions)
                {
                    // if (x == 0 && y == 0 && z == 0)
                    // {
                    //     continue;
                    // }

                    const glm::ivec3 otherOffsetRelative {x, y, z};

                    const glm::ivec3 otherChunkPosition {
                        position + otherOffsetRelative};

                    const ChunkCoordinate coord {glm::i32vec3 {
                        util::divideEuclidean(otherChunkPosition.x, 64),
                        util::divideEuclidean(otherChunkPosition.y, 64),
                        util::divideEuclidean(otherChunkPosition.z, 64),
                    }};

                    if (auto maybeChunkIt = this->global_chunks.find(coord);
                        maybeChunkIt != this->global_chunks.cend())
                    {
                        const glm::ivec3 indexVectorThis {
                            otherOffsetRelative / 64 + 1};
                        const glm::ivec3 indexVectorOther {
                            -otherOffsetRelative / 64 + 1};

                        const u32 otherChunkId {maybeChunkIt->second.id};

                        // NOLINTNEXTLINE
                        gpuChunkData.adjacent_chunks[indexVectorThis.x]
                                                    [indexVectorThis.y]
                                                    [indexVectorThis.z] =
                            otherChunkId;

                        this->gpu_chunk_data.modify(otherChunkId)
                            .adjacent_chunks[indexVectorOther.x]
                                            [indexVectorOther.y]
                                            [indexVectorOther.z] = thisChunkId;
                    }
                }
            }
        }

        this->gpu_chunk_data.write(thisChunkId, {&gpuChunkData, 1});

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

// we need to remove references to ourself in the adjacent chunks that
// border this
#warning implement unlinking
        // for (xyz)
        // {
        //     u32 otherChunkId = func(xyz);

        //     this->gpu_chunk_data.modify(otherChunkId)
        //         .adjacent_chunks[otherindexvector] = ~0;
        // }

        std::optional<std::array<util::RangeAllocation, 6>>& maybeFacesToFree =
            this->cpu_chunk_data[toFree.id].face_data;

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
        this->cpu_chunk_data[c.id].needs_remesh = true;

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
            this->opacity_bricks.modify(maybeBrickPointer.pointer).fill(false);

            // NOLINTNEXTLINE
            this->brick_maps.modify(c.id).data[bC.x][bC.y][bC.z] =
                maybeBrickPointer;
        }

        // NOLINTNEXTLINE
        this->material_bricks.modify(maybeBrickPointer.pointer)
            .data[bP.x][bP.y][bP.z] = v;

        if (v == Voxel::NullAirEmpty)
        {
            this->opacity_bricks.modify(maybeBrickPointer.pointer)
                .write(bP, false);
        }
        else
        {
            this->opacity_bricks.modify(maybeBrickPointer.pointer)
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
    //                         this->opacity_bricks.read(ptr.pointer, 1)[0];

    //                     thisBrick.iterateOverVoxels(
    //                         [&](BrickLocalPosition bP, bool isFilled)
    //                         {
    //                             VoxelFaceDirection dir =
    //                                 static_cast<VoxelFaceDirection>(
    //                                     normalDirection);

    //                             ChunkLocalPosition pos =
    //                                 assembleChunkLocalPosition(bC, bP);

    //                             std::optional<ChunkLocalPosition> adjPos
    //                             =
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
    //                                         MaybeBrickPointer
    //                                         adjBrickPointer
    //                                         =
    //                                             thisBrickMap
    //                                                 .data[adjBC.x][adjBC.y]
    //                                                      [adjBC.z];

    //                                         if
    //                                         (!adjBrickPointer.isNull())
    //                                         {
    //                                             if (!this->opacity_bricks
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
    //             this->voxel_faces.getDataNonCoherent().data() +
    //             a.offset);
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
                    const OpacityBrick& thisBrick =
                        this->opacity_bricks.read(ptr.pointer, 1)[0];

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
        std::unique_ptr<DenseBitChunk> thisChunkData =
            this->makeDenseBitChunk(chunkId);

        std::array<util::RangeAllocation, 6> outAllocations {};

        auto makeChunkSlice = [&](u32 normalId, u64 offset) -> ChunkSlice
        {
            VoxelFaceDirection dir = static_cast<VoxelFaceDirection>(normalId);
            const auto [widthAxis, heightAxis, ascensionAxis] =
                getDrivingAxes(dir);
            glm::i8vec3 normal = getDirFromDirection(dir);

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
                                + static_cast<i8>(offset) * ascensionAxis
                                + normal)
                            && thisChunkData->isOccupied(
                                w * widthAxis + h * heightAxis
                                + static_cast<i8>(offset) * ascensionAxis
                                + normal))
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
        for (util::RangeAllocation& thisAllocation : outAllocations)
        {
            std::vector<GreedyVoxelFace> faces {};

            for (u64 ascend = 0; ascend < 64; ++ascend)
            {
                ChunkSlice thisSlice = makeChunkSlice(normalId, ascend);

                for (u64 height = 0; height < 64; ++height)
                {
                    // NOLINTNEXTLINE
                    for (u64 width = std::countr_zero(thisSlice.data[height]);
                         width < 64;
                         ++width)
                    {
                        // NOLINTNEXTLINE
                        if ((thisSlice.data[height] & (UINT64_C(1) << width))
                            != 0ULL)
                        {
                            const u64 faceWidth =
                                static_cast<u64>(std::countr_one(
                                    // NOLINTNEXTLINE
                                    thisSlice.data[height] >> width));

                            VoxelFaceDirection dir =
                                static_cast<VoxelFaceDirection>(normalId);
                            const auto [widthAxis, heightAxis, ascensionAxis] =
                                getDrivingAxes(dir);

                            glm::i8vec3 thisRoot =
                                ascensionAxis * static_cast<i8>(ascend)
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

                            util::assertFatal(faceHeight != 0, "hmmm");

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

                            // goto next_layer;
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

            thisAllocation = this->voxel_face_allocator.allocate(
                static_cast<u32>(faces.size()));
            // util::logTrace("allocating {}", thisAllocation.offset);

            std::copy(
                faces.cbegin(),
                faces.cend(),
                this->voxel_faces.getDataNonCoherent().data()
                    + thisAllocation.offset);
            const gfx::vulkan::FlushData flush {
                .offset_elements {thisAllocation.offset},
                .size_elements {faces.size()},
            };
            this->voxel_faces.flush({&flush, 1});

            normalId += 1;
        }

        return outAllocations;
    }

    std::array<util::RangeAllocation, 6>
    ChunkManager::meshChunkLinear(u32 chunkId) // NOLINT
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
