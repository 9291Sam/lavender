#include "laser_particle_system.hpp"
#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "shaders/include/common.glsl"
#include "util/log.hpp"
#include "util/range_allocator.hpp"
#include "util/static_filesystem.hpp"
#include <glm/common.hpp>
#include <glm/gtx/hash.hpp>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace render
{
    static constexpr std::size_t MaxParticles = 1u << 20u;
    static constexpr std::size_t MaxHandles   = 100;

    LaserParticleSystem::LaserParticleSystem(const gfx::Renderer* renderer_)
        : renderer {renderer_}
        , handle_allocator(MaxHandles)
        , configs {}
        , buffer_position_allocator(MaxParticles, MaxHandles)
        , particles_buffer(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxParticles,
              "LaserParticleSystem Particles Buffer")
        , draw_indirect_commands(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxHandles,
              "LaserParticleSystem Draw Indirect Buffer")
        , particle_render_descriptor_set_layout(
              this->renderer->getAllocator()->cacheDescriptorSetLayout(
                  gfx::vulkan::CacheableDescriptorSetLayoutCreateInfo {
                      .bindings {
                          vk::DescriptorSetLayoutBinding {
                              .binding {0},
                              .descriptorType {vk::DescriptorType::eStorageBuffer},
                              .descriptorCount {1},
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment},
                              .pImmutableSamplers {nullptr},
                          },
                      },
                      .name {"Particle Render Descriptor Set Layout"}}))
        , particle_render_descriptor_set {this->renderer->getAllocator()->allocateDescriptorSet(
              **this->particle_render_descriptor_set_layout, "Particle Render Descriptor Set")}
        , particle_render_pipeline {this->renderer->getAllocator()->cachePipeline(

              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("laser.vert"), "Laser Vertex Shader")},
                          .entry_point {"main"},
                      },
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {this->renderer->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("laser.frag"), "Laser Fragment Shader")},
                          .entry_point {"main"},
                      },
                  }},
                  .vertex_attributes {},
                  .vertex_bindings {},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .discard_enable {false},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eClockwise},
                  .depth_test_enable {true},
                  .depth_write_enable {true},
                  .depth_compare_op {vk::CompareOp::eGreater},
                  .color_format {gfx::Renderer::ColorFormat.format},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .blend_enable {true},
                  .layout {this->renderer->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {{this->particle_render_descriptor_set_layout}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment},
                              .offset {0},
                              .size {128}}},
                          .name {"Laser Pipeline Layout"}})},
                  .name {"Laser Pipeline"}})}
    {
        const vk::DescriptorBufferInfo bufferInfo {
            .buffer {*this->particles_buffer}, .offset {0}, .range {vk::WholeSize}};

        const vk::WriteDescriptorSet write {
            .sType {vk::StructureType::eWriteDescriptorSet},
            .pNext {nullptr},
            .dstSet {this->particle_render_descriptor_set},
            .dstBinding {0},
            .dstArrayElement {0},
            .descriptorCount {1},
            .descriptorType {vk::DescriptorType::eStorageBuffer},
            .pImageInfo {nullptr},
            .pBufferInfo {&bufferInfo},
            .pTexelBufferView {},
        };

        this->renderer->getDevice()->getDevice().updateDescriptorSets(1, &write, 0, nullptr);
    }

    LaserParticleSystem::~LaserParticleSystem() = default;

    LaserParticleSystem::LaserHandle LaserParticleSystem::createParticleSystem(
        LaserParticleSystem::ParticleSystemConfiguration newConfig)
    {
        std::mt19937_64                       gen {std::random_device {}()};
        std::uniform_real_distribution<float> uniformDistanceAlong {0.0f, 1.0f};
        std::normal_distribution<float>       uniformLocalAxis1 {0.0f, 1.0f};
        std::normal_distribution<float>       uniformLocalAxis2 {0.0f, 1.0f};

        const float     beamLength = glm::distance(newConfig.start, newConfig.end);
        const glm::vec3 forward    = glm::normalize(newConfig.end - newConfig.start);
        const glm::vec3 right      = glm::normalize(glm::cross(forward, game::Transform::UpVector));
        const glm::vec3 up         = glm::normalize(glm::cross(forward, right)); // excessive

        const std::size_t numberOfParticles =
            static_cast<std::size_t>(newConfig.density * beamLength);

        std::vector<LaserParticleInfo> particles {};
        particles.resize(numberOfParticles);

        for (LaserParticleInfo& p : particles)
        {
            const glm::vec3 positionAlongBeam =
                uniformDistanceAlong(gen) * beamLength * forward + newConfig.start;
            const glm::vec3 spreadAlongBeam = uniformLocalAxis1(gen) * newConfig.spread * right
                                            + uniformLocalAxis2(gen) * newConfig.spread * up;

            const glm::vec3 particlePosition = positionAlongBeam + spreadAlongBeam;

            p = LaserParticleInfo {
                .position_x {particlePosition.x},
                .position_y {particlePosition.y},
                .position_z {particlePosition.z},
                .scale {newConfig.scale},
                .color {gpu_linearToSRGB(newConfig.color)}};
        }

        LaserHandle newHandle = this->handle_allocator.allocateOrPanic();

        util::RangeAllocation newAllocation =
            this->buffer_position_allocator.allocate(static_cast<u32>(numberOfParticles));

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        this->configs[this->handle_allocator.getValueOfHandle(newHandle)] = newAllocation;

        this->particles_buffer.uploadImmediate(newAllocation.offset, particles);

        util::logTrace("Created particle system with {} particles", numberOfParticles);

        return newHandle;
    }

    void LaserParticleSystem::destroyParticleSystem(LaserHandle oldHandle)
    {
        std::optional<util::RangeAllocation> shouldBeCurrentAllocation = std::exchange(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            this->configs[this->handle_allocator.getValueOfHandle(oldHandle)],
            std::nullopt);

        if (shouldBeCurrentAllocation.has_value())
        {
            this->buffer_position_allocator.free(*shouldBeCurrentAllocation);
        }
        else
        {
            util::panic("Impossible");
        }

        this->handle_allocator.free(std::move(oldHandle));
    }

    game::FrameGenerator::RecordObject
    LaserParticleSystem::getRecordObject(const game::Camera& camera, const game::Game& game)
    {
        std::vector<vk::DrawIndirectCommand> newDrawCommands {};

        this->handle_allocator.iterateThroughAllocatedElements(
            [&](const u8 validHandle)
            {
                const std::optional<util::RangeAllocation>& shouldBeCurrentAllocation =
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    this->configs[validHandle];

                util::assertFatal(shouldBeCurrentAllocation.has_value(), "impossible");

                newDrawCommands.push_back(vk::DrawIndirectCommand {
                    .vertexCount {
                        this->buffer_position_allocator.getSizeOfAllocation(
                            *shouldBeCurrentAllocation)
                        * 6},
                    .instanceCount {1},
                    .firstVertex {shouldBeCurrentAllocation->offset},
                    // Smuggling a u32 that's our offset in the data array
                    .firstInstance {0}});
            });

        this->renderer->getStager().enqueueTransfer(
            this->draw_indirect_commands,
            0,
            std::span<const vk::DrawIndirectCommand> {newDrawCommands});

        struct PushConstants
        {
            glm::mat4 mvp;
            glm::vec4 camera_forward;
            glm::vec4 camera_right;
            glm::vec4 camera_up;
            u32       random_seed;
        };

        const float timeBetweenFlash = 0.1f;

        const PushConstants pc {
            .mvp {camera.getPerspectiveMatrix(game, {})},
            .camera_forward {glm::vec4 {camera.getForwardVector(), 0.0}},
            .camera_right {glm::vec4 {camera.getRightVector(), 0.0}},
            .camera_up {glm::vec4 {camera.getUpVector(), 0.0}},
            .random_seed {gpu_hashU32(
                static_cast<u32>(this->renderer->getTimeAlive() * 1.0f / timeBetweenFlash))}};

        return game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {this->particle_render_pipeline},
            .descriptors {this->particle_render_descriptor_set, nullptr, nullptr, nullptr},
            .record_func {[buf         = *this->draw_indirect_commands,
                           localPC     = pc,
                           sizeOfDraws = static_cast<u32>(newDrawCommands.size())](
                              vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, u32)
                          {
                              commandBuffer.pushConstants<PushConstants>(
                                  layout,
                                  vk::ShaderStageFlagBits::eVertex
                                      | vk::ShaderStageFlagBits::eFragment,
                                  0,
                                  localPC);

                              commandBuffer.drawIndirect(
                                  buf, 0, sizeOfDraws, sizeof(vk::DrawIndirectCommand));
                          }}};
    }
} // namespace render