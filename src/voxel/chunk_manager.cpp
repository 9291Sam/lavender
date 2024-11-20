#include "voxel/chunk_manager.hpp"
#include "brick_map.hpp"
#include "brick_pointer.hpp"
#include "brick_pointer_allocator.hpp"
#include "chunk.hpp"
#include "chunk_manager.hpp"
#include "constants.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/device.hpp"
#include "gfx/window.hpp"
#include "greedy_voxel_face.hpp"
#include "opacity_brick.hpp"
#include "point_light.hpp"
#include "util/index_allocator.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/range_allocator.hpp"
#include "util/static_filesystem.hpp"
#include "util/timer.hpp"
#include "voxel/brick_pointer.hpp"
#include "voxel/constants.hpp"
#include "voxel/dense_bit_chunk.hpp"
#include "voxel/material_manager.hpp"
#include "voxel/opacity_brick.hpp"
#include "voxel/visibility_brick.hpp"
#include "voxel_face_direction.hpp"
#include <algorithm>
#include <bit>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <cstddef>
#include <future>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <memory>
#include <optional>
#include <ranges>
#include <source_location>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace voxel
{

    std::array<std::vector<GreedyVoxelFace>, 6>
    meshChunkGreedy(std::unique_ptr<DenseBitChunk> thisChunkData);

    static constexpr u32 MaxChunks          = 4096;
    static constexpr u32 DirectionsPerChunk = 6;
    static constexpr u32 MaxBricks          = 131072 * 8;
    static constexpr u32 MaxFaces           = 1048576 * 4;
    static constexpr u32 MaxLights          = 4096;
    static constexpr u32 MaxFaceIdMapNodes  = 1U << 24U;

    ChunkManager::ChunkManager(const game::Game* game)
        : renderer {game->getRenderer()}
        , chunk_id_allocator {MaxChunks}
        , cpu_chunk_data {MaxChunks, CpuChunkData {}}
        , gpu_chunk_data(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              MaxChunks,
              "Gpu Chunk Data Buffer")
        , brick_maps(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              MaxChunks,
              "Brick Maps")
        , indirect_payload(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Payload")
        , indirect_commands(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxChunks * DirectionsPerChunk),
              "Indirect Commands")
        , brick_allocator(MaxBricks)
        , brick_parent_info(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks),
              "Brick Parent Info")
        , material_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks),
              "Material Bricks")
        , opacity_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxBricks),
              "Opacity Bricks")
        , visibility_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxBricks),
              "Visibility Bricks")
        , face_id_map(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaceIdMapNodes),
              "Face Id Map")
        , voxel_face_allocator {MaxFaces, MaxChunks * 6}
        , voxel_faces(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              static_cast<std::size_t>(MaxFaces),
              "Voxel Faces")
        , material_buffer {voxel::generateVoxelMaterialBuffer(this->renderer)}
        , global_voxel_data(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
                  | vk::BufferUsageFlagBits::eIndirectBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Global Voxel Data")
        , visible_face_data(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxFaces),
              "Visible Face Data")
        , light_allocator {MaxLights}
        , lights_buffer(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              static_cast<std::size_t>(MaxLights),
              "Lights Buffer")
        , global_chunks_buffer(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Global Chunks Buffer")
        , descriptor_set_layout {this->renderer->getAllocator()->cacheDescriptorSetLayout(
              gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {
                  .bindings {{
                      vk::DescriptorSetLayoutBinding {
                          .binding {0},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {1},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {2},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {3},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {4},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {5},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {6},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },

                      vk::DescriptorSetLayoutBinding {
                          .binding {7},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {8},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },

                      vk::DescriptorSetLayoutBinding {
                          .binding {9},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {10},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {11},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
                              | vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                      vk::DescriptorSetLayoutBinding {
                          .binding {12},
                          .descriptorType {vk::DescriptorType::eStorageBuffer},
                          .descriptorCount {1},
                          .stageFlags {vk::ShaderStageFlagBits::eCompute},
                          .pImmutableSamplers {nullptr},
                      },
                  }},
                  .name {"Voxel Descriptor Set Layout"}})}
        , voxel_render_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_render.vert"),
                              "Voxel Render Vertex Shader")},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_render.frag"),
                              "Voxel Render Fragment Shader")},
                          .entry_point {"main"}},
                  }},
                  .vertex_attributes {{
                      vk::VertexInputAttributeDescription {
                          .location {0},
                          .binding {0},
                          .format {vk::Format::eR32G32B32A32Sfloat},
                          .offset {offsetof(ChunkDrawIndirectInstancePayload, position)}},
                      vk::VertexInputAttributeDescription {
                          .location {1},
                          .binding {0},
                          .format {vk::Format::eR32Uint},
                          .offset {offsetof(ChunkDrawIndirectInstancePayload, normal)}},
                      vk::VertexInputAttributeDescription {
                          .location {2},
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
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {vk::ShaderStageFlagBits::eVertex},
                              .offset {0},
                              .size {sizeof(u32)},

                          }},
                          .name {"Voxel Render Pipeline Layout"}})},
                  .name {"Voxel Render Pipeline"}})}
        , voxel_visibility_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {this->renderer->getAllocator()->cacheShaderModule(
                      staticFilesystem::loadShader("voxel_visibility_detection.comp"),
                      "Voxel Visibility Detection Compute Shader")},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Visibility Detection Pipeline Layout"}})},
                  .name {"Voxel Visibility Detection Pipeline"}})}
        , voxel_color_calculation_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableComputePipelineCreateInfo {
                  .entry_point {"main"},
                  .shader {this->renderer->getAllocator()->cacheShaderModule(
                      staticFilesystem::loadShader("voxel_color_calculation.comp"),
                      "Voxel Color Calculation Compute Shader")},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Color Calculation Pipeline Layout"}})},
                  .name {"Voxel Color Calculation Pipeline"},
              })}
        , voxel_color_transfer_pipeline {this->renderer->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("voxel_color_transfer.vert"),
                              "Voxel Color Transfer Vertex "
                              "Shader")},
                          .entry_point {"main"}},
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
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
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {
                              {game->getGlobalInfoDescriptorSetLayout(),
                               this->descriptor_set_layout}},
                          .push_constants {},
                          .name {"Voxel Color Transfer Pipeline Layout"}})},
                  .name {"Voxel Color Transfer Pipeline"}})}
        , chunk_descriptor_set {this->renderer->getAllocator()->allocateDescriptorSet(
              **this->descriptor_set_layout, "Voxel Descriptor Set")}
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
                .buffer {*this->face_id_map},
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
                .buffer {*this->global_voxel_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->visible_face_data},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->lights_buffer},
                .offset {0},
                .range {vk::WholeSize},
            },
            vk::DescriptorBufferInfo {
                .buffer {*this->global_chunks_buffer},
                .offset {0},
                .range {vk::WholeSize},
            },
        };

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

        std::array<std::array<std::array<u16, 256>, 256>, 256>* gpuChunksBufferPtr =
            &this->global_chunks_buffer.modify(0);

        const std::size_t len = sizeof(*gpuChunksBufferPtr);
        std::memset(gpuChunksBufferPtr, -1, len);

        this->face_id_map.fillImmediate(FaceIdBrickHashMapStorage {.key {~0U}, .value {~0U}});
        this->visibility_bricks.fillImmediate(VisibilityBrick {});

        this->renderer->getDevice()->getDevice().updateDescriptorSets(
            static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

        const std::size_t bytesAllocated = gfx::vulkan::bufferBytesAllocated.load();
        const double      gibAllocated = static_cast<double>(bytesAllocated) / (1024 * 1024 * 1024);

        util::logTrace("allocated {:.3} GiB", gibAllocated);
    }

    ChunkManager::~ChunkManager() = default;

    std::vector<game::FrameGenerator::RecordObject>
    // NOLINTNEXTLINE
    ChunkManager::makeRecordObject(
        const game::Game* game, const gfx::vulkan::BufferStager& stager, game::Camera c)
    {
        std::vector<vk::DrawIndirectCommand>          indirectCommands {};
        std::vector<ChunkDrawIndirectInstancePayload> indirectPayload {};

        u32 callNumber = 0;

        this->chunk_id_allocator.iterateThroughAllocatedElements(
            [&, this](u32 chunkId)
            {
                CpuChunkData& thisChunkData = this->cpu_chunk_data[chunkId];

                switch (static_cast<u32>(thisChunkData.needs_remesh) << 1
                        | static_cast<u32>(thisChunkData.is_remesh_in_progress))
                {
                case 0b00:
                    break; // do nothing
                case 0b01:
                    [[fallthrough]];
                case 0b11: {
                    if (thisChunkData.future_mesh.valid())
                    {
                        // free old stuff if it exists
                        if (thisChunkData.face_data.has_value())
                        {
                            for (util::RangeAllocation a : *thisChunkData.face_data)
                            {
                                this->voxel_face_allocator.free(a);
                            }
                        }

                        // grab all of the new data, allocate gpu memory for it and move everything
                        // there
                        thisChunkData.is_remesh_in_progress = false;

                        const std::array<std::vector<GreedyVoxelFace>, 6>& greedyFaces =
                            thisChunkData.future_mesh.get();

                        std::array<util::RangeAllocation, 6> allocations {};

                        for (auto [thisAllocation, faces] :
                             std::views::zip(allocations, greedyFaces))
                        {
                            thisAllocation =
                                this->voxel_face_allocator.allocate(static_cast<u32>(faces.size()));

                            this->voxel_faces.write(thisAllocation.offset, faces);
                        }

                        thisChunkData.face_data = allocations;
                    }
                }
                break; // keep waiting
                case 0b10: {
                    thisChunkData.is_remesh_in_progress = true;

                    BrickList chunkData = this->formBrickList(chunkId);

                    thisChunkData.future_mesh = std::async(
                        [](BrickList localData) // NOLINT: lifetimes exist
                        {
                            std::unique_ptr<DenseBitChunk> denseBitData =
                                localData.formDenseBitChunk();

                            return meshChunkGreedy(std::move(denseBitData));
                        },
                        std::move(chunkData));

                    thisChunkData.needs_remesh = false;
                    break;
                }
                }

                // TODO: better opacity tests cpu-side culling
                bool isChunkInFrustum = false;

                isChunkInFrustum |=
                    (glm::distance(c.getPosition(), thisChunkData.position.xyz())
                     < ChunkEdgeLengthVoxels * 2);

                isChunkInFrustum |=
                    (glm::all(glm::lessThan(
                         c.getPosition() - thisChunkData.position.xyz(),
                         glm::vec3 {ChunkEdgeLengthVoxels}))
                     && glm::all(glm::greaterThan(
                         c.getPosition() - thisChunkData.position.xyz(), glm::vec3 {0})));

                for (auto x :
                     {thisChunkData.position.x,
                      //   thisChunkData.position.x + ChunkEdgeLengthVoxels / 2.0f,
                      thisChunkData.position.x + ChunkEdgeLengthVoxels})
                {
                    for (auto y :
                         {thisChunkData.position.y,
                          //   thisChunkData.position.y + ChunkEdgeLengthVoxels / 2.0f,
                          thisChunkData.position.y + ChunkEdgeLengthVoxels})
                    {
                        for (auto z :
                             {thisChunkData.position.z,
                              //   thisChunkData.position.z + ChunkEdgeLengthVoxels / 2.0f,
                              thisChunkData.position.z + ChunkEdgeLengthVoxels})
                        {
                            glm::vec4 cornerPos {x, y, z, 1.0};

                            auto res = c.getPerspectiveMatrix(
                                           *game, game::Transform {.translation {cornerPos}})
                                     * glm::vec4 {0.0, 0.0, 0.0, 1.0};

                            if (glm::all(glm::lessThan(res.xyz() / res.w, glm::vec3 {1.25}))
                                && glm::all(glm::greaterThan(res.xyz() / res.w, glm::vec3 {-1.25})))
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
                            .vertexCount {this->voxel_face_allocator.getSizeOfAllocation(a) * 6},
                            .instanceCount {1},
                            .firstVertex {a.offset * 6},
                            .firstInstance {callNumber},
                        });

                        indirectPayload.push_back(ChunkDrawIndirectInstancePayload {
                            .position {thisChunkData.position},
                            .normal {normal},
                            .chunk_id {chunkId}});

                        callNumber += 1;
                        normal += 1;
                    }
                }
            });

        this->gpu_chunk_data.flushViaStager(stager);
        this->brick_maps.flushViaStager(stager);
        this->brick_parent_info.flushViaStager(stager);
        this->material_bricks.flushViaStager(stager);
        this->voxel_faces.flushViaStager(stager);
        this->opacity_bricks.flushViaStager(stager);
        this->lights_buffer.flushViaStager(stager);
        this->global_chunks_buffer.flushViaStager(stager);

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

        game::FrameGenerator::RecordObject update = game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::PreFrameUpdate},
            .pipeline {nullptr},
            .descriptors {},
            .record_func {[this, indirectCommands, indirectPayload](
                              vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              GlobalVoxelData data {
                                  .number_of_visible_faces {},
                                  .number_of_calculating_draws_x {},
                                  .number_of_calculating_draws_y {},
                                  .number_of_calculating_draws_z {},
                                  .number_of_lights {
                                      this->light_allocator.getUpperBoundOnAllocatedElements()},
                              };

                              commandBuffer.updateBuffer(
                                  *this->global_voxel_data, 0, sizeof(GlobalVoxelData) - 4, &data);
                          }}};

        numberOfFacesVisible =
            this->global_voxel_data.getGpuDataNonCoherent()[0].readback_number_of_visible_faces;
        numberOfFacesPossible = MaxFaceIdMapNodes;

        numberOfChunksAllocated = this->chunk_id_allocator.getNumberAllocated();
        numberOfChunksPossible  = MaxChunks;

        numberOfBricksAllocated = this->brick_allocator.getNumberAllocated();
        numberOfBricksPossible  = MaxBricks;

        game::FrameGenerator::RecordObject chunkDraw = game::FrameGenerator::RecordObject {
            .transform {game::Transform {}},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelRenderer},
            .pipeline {this->voxel_render_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->chunk_descriptor_set, nullptr, nullptr}},
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
                {this->global_descriptor_set, this->chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[this](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              vk::Extent2D fbSize =
                                  this->renderer->getWindow()->getFramebufferSize();

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
                {this->global_descriptor_set, this->chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[this](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              commandBuffer.dispatchIndirect(*this->global_voxel_data, 4);
                          }}};

        game::FrameGenerator::RecordObject colorTransfer = game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::VoxelColorTransfer},
            .pipeline {this->voxel_color_transfer_pipeline},
            .descriptors {
                {this->global_descriptor_set, this->chunk_descriptor_set, nullptr, nullptr}},
            .record_func {[](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              commandBuffer.draw(3, 1, 0, 0);
                          }}};

        return {update, chunkDraw, visibilityDraw, colorCalculation, colorTransfer};
    }

    PointLight ChunkManager::createPointLight()
    {
        std::expected<u32, util::IndexAllocator::OutOfBlocks> maybeNewPointLightId =
            this->light_allocator.allocate();

        util::assertFatal(maybeNewPointLightId.has_value(), "Failed to allocate new point light!");

        this->lights_buffer.modify(*maybeNewPointLightId) = InternalPointLight {};

        return PointLight {*maybeNewPointLightId};
    }

    void ChunkManager::modifyPointLight(
        const PointLight& l, glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs)
    {
        this->lights_buffer.modify(l.id) = InternalPointLight {
            .position {position.xyzz()}, .color_and_power {colorAndPower}, .falloffs {falloffs}};
    }

    void ChunkManager::destroyPointLight(PointLight toFree)
    {
        this->light_allocator.free(toFree.id);

        this->lights_buffer.modify(toFree.id) = InternalPointLight {};

        toFree.id = PointLight::NullId;
    }

    Chunk ChunkManager::createChunk(ChunkCoordinate coordinate)
    {
        const std::expected<u32, util::IndexAllocator::OutOfBlocks> maybeThisChunkId =
            this->chunk_id_allocator.allocate();

        if (!maybeThisChunkId.has_value())
        {
            util::panic("Failed to allocate new chunk!");
        }

        const u32 thisChunkId = *maybeThisChunkId;

        // TODO: no overallocation protection...
        this->cpu_chunk_data[thisChunkId] = CpuChunkData {
            .position {glm::vec4 {
                coordinate.x * ChunkEdgeLengthVoxels,
                coordinate.y * ChunkEdgeLengthVoxels,
                coordinate.z * ChunkEdgeLengthVoxels,
                0.0}},
            .face_data {std::nullopt},
            .needs_remesh {true}};

        this->brick_maps.write(thisChunkId, BrickMap {});
        this->gpu_chunk_data.write(
            thisChunkId,
            GpuChunkData {.position {glm::vec4 {
                coordinate.x * ChunkEdgeLengthVoxels,
                coordinate.y * ChunkEdgeLengthVoxels,
                coordinate.z * ChunkEdgeLengthVoxels,
                0.0}}});

        util::assertFatal(coordinate.x > -127 && coordinate.x < 127, "{}", coordinate.x);
        util::assertFatal(coordinate.y > -127 && coordinate.y < 127, "{}", coordinate.y);
        util::assertFatal(coordinate.z > -127 && coordinate.z < 127, "{}", coordinate.z);

        // NOLINTNEXTLINE
        this->global_chunks_buffer.modify(0)[static_cast<std::size_t>(coordinate.x) + 128]
                                            [static_cast<std::size_t>(coordinate.y) + 128]
                                            [static_cast<std::size_t>(coordinate.z) + 128] =
            static_cast<u16>(*maybeThisChunkId);

        return Chunk {thisChunkId};
    }

    void ChunkManager::destroyChunk(Chunk toFree)
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
                        this->brick_allocator.free(ptr.pointer);
                    }
                }
            }
        }

        CpuChunkData& cpuChunkData = this->cpu_chunk_data[toFree.id];

        std::optional<std::array<util::RangeAllocation, 6>>& maybeFacesToFree =
            cpuChunkData.face_data;

        if (maybeFacesToFree.has_value())
        {
            for (util::RangeAllocation allocation : *maybeFacesToFree)
            {
                this->voxel_face_allocator.free(allocation);
            }
        }

        ChunkCoordinate coord {glm::i32vec3 {
            util::divideEuclidean(static_cast<i32>(cpuChunkData.position.x), 64),
            util::divideEuclidean(static_cast<i32>(cpuChunkData.position.y), 64),
            util::divideEuclidean(static_cast<i32>(cpuChunkData.position.z), 64),
        }};

        // NOLINTNEXTLINE
        this->global_chunks_buffer.modify(
            0)[static_cast<u64>(coord.x) + 128][static_cast<u64>(coord.y) + 128]
              [static_cast<u64>(coord.z) + 128] = static_cast<u16>(-1);

        toFree.id = Chunk::NullChunk;
    }

    void ChunkManager::writeVoxelToChunk(const Chunk& c, std::span<const VoxelWrite> writes)
    {
        this->cpu_chunk_data[c.id].needs_remesh = true;

        for (const VoxelWrite& w : writes)
        {
            util::assertFatal(
                w.position.x < ChunkEdgeLengthVoxels && w.position.y < ChunkEdgeLengthVoxels
                    && w.position.z < ChunkEdgeLengthVoxels,
                "ChunkManager::writeVoxelToChunk position [{}, {}, {}] out of "
                "bounds",
                static_cast<u8>(w.position.x),
                static_cast<u8>(w.position.y),
                static_cast<u8>(w.position.z));

            const auto [bC, bP] = splitChunkLocalPosition(w.position);

            MaybeBrickPointer maybeBrickPointer =
                this->brick_maps.read(c.id, 1)[0].data[bC.x][bC.y][bC.z]; // NOLINT

            // TODO: fix all of these modify calls, this isn't great...
            if (maybeBrickPointer.isNull())
            {
                const BrickPointer newBrickPointer =
                    BrickPointer {this->brick_allocator.allocate().value()};

                maybeBrickPointer = newBrickPointer;

                this->brick_parent_info.modify(maybeBrickPointer.pointer) = BrickParentInformation {
                    .parent_chunk {c.id},
                    .position_in_parent_chunk {bC.getLinearPositionInChunk()}};
                this->material_bricks.modify(maybeBrickPointer.pointer).fill(Voxel::NullAirEmpty);
                this->opacity_bricks.modify(maybeBrickPointer.pointer).fill(false);

                // NOLINTNEXTLINE
                this->brick_maps.modify(c.id).data[bC.x][bC.y][bC.z] = maybeBrickPointer;
            }

            // NOLINTNEXTLINE
            this->material_bricks.modify(maybeBrickPointer.pointer).data[bP.x][bP.y][bP.z] =
                w.voxel;

            if (w.voxel == Voxel::NullAirEmpty)
            {
                this->opacity_bricks.modify(maybeBrickPointer.pointer).write(bP, false);
            }
            else
            {
                this->opacity_bricks.modify(maybeBrickPointer.pointer).write(bP, true);
            }
        }
    }

    bool ChunkManager::areAnyVoxelsOccupied(
        const Chunk& c, std::span<const ChunkLocalPosition> positions)
    {
        for (const ChunkLocalPosition& p : positions) // NOLINT
        {
            auto [bC, bP] = splitChunkLocalPosition(p);

            MaybeBrickPointer maybeBrickPointer =
                this->brick_maps.read(c.id).data[bC.x][bC.y][bC.z]; // NOLINT

            if (maybeBrickPointer.isNull())
            {
                continue;
            }
            else
            {
                if (this->opacity_bricks.read(maybeBrickPointer.pointer).read(bP))
                {
                    return true;
                }
            }
        }

        return false;
    }

    boost::dynamic_bitset<u64> ChunkManager::readVoxelFromChunkOpacity(
        const Chunk& c, std::span<const ChunkLocalPosition> positions)
    {
        boost::dynamic_bitset<u64> out {};
        out.reserve(positions.size());

        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            auto [bC, bP] = splitChunkLocalPosition(positions[i]);

            MaybeBrickPointer maybeBrickPointer =
                this->brick_maps.read(c.id).data[bC.x][bC.y][bC.z]; // NOLINT

            if (!maybeBrickPointer.isNull()
                && this->opacity_bricks.read(maybeBrickPointer.pointer).read(bP))
            {
                out.set(i);
            }
        }

        return out;
    }

    std::vector<Voxel> ChunkManager::readVoxelFromChunkMaterial(
        const Chunk& c, std::span<const ChunkLocalPosition> positions)
    {
        std::vector<Voxel> out {};
        out.reserve(positions.size());

        for (const ChunkLocalPosition& p : positions)
        {
            auto [bC, bP] = splitChunkLocalPosition(p);

            MaybeBrickPointer maybeBrickPointer =
                this->brick_maps.read(c.id).data[bC.x][bC.y][bC.z]; // NOLINT

            if (!maybeBrickPointer.isNull())
            {
                Voxel v = this->material_bricks.read(maybeBrickPointer.pointer).read(bP);

                out.push_back(v);
            }
            else
            {
                out.push_back(Voxel::NullAirEmpty);
            }
        }

        return out;
    }

    ChunkManager::BrickList ChunkManager::formBrickList(u32 chunkId)
    {
        BrickList out {};

        const BrickMap& thisBrickMap = this->brick_maps.read(chunkId);

        thisBrickMap.iterateOverPointers(
            // NOLINTNEXTLINE
            [&](BrickCoordinate bC, MaybeBrickPointer ptr)
            {
                if (!ptr.isNull())
                {
                    out.data.push_back({bC, this->opacity_bricks.read(ptr.pointer)});
                }
            });

        return out;
    }

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
                            const auto [widthAxis, heightAxis, ascensionAxis] = getDrivingAxes(dir);

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

} // namespace voxel
