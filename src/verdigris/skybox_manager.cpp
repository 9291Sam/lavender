#include "skybox_manager.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/renderer.hpp"
#include "util/static_filesystem.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace verdigris
{
    SkyboxManager::SkyboxManager(const game::Game* game_)
        : pipeline {game_->getRenderer()->getAllocator()->cachePipeline(
              gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                  .stages {{
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eVertex},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("skybox.vert"), "Skybox Vertex Shader")},
                          .entry_point {"main"},
                      },
                      gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                          .stage {vk::ShaderStageFlagBits::eFragment},
                          .shader {game_->getRenderer()->getAllocator()->cacheShaderModule(
                              staticFilesystem::loadShader("skybox.frag"),
                              "Skybox Fragment Shader")},
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
                  .depth_write_enable {false},
                  .depth_compare_op {vk::CompareOp::eLess},
                  .color_format {gfx::Renderer::ColorFormat.format},
                  .depth_format {gfx::Renderer::DepthFormat},
                  .blend_enable {true},
                  .layout {game_->getRenderer()->getAllocator()->cachePipelineLayout(
                      gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                          .descriptors {{game_->getGlobalInfoDescriptorSetLayout()}},
                          .push_constants {vk::PushConstantRange {
                              .stageFlags {
                                  vk::ShaderStageFlagBits::eVertex
                                  | vk::ShaderStageFlagBits::eFragment},
                              .offset {0},
                              .size {128}}},
                          .name {"Skybox Pipeline Layout"}})},
                  .name {"Skybox Pipeline"}})}
        , game {game_}
    {}

    game::FrameGenerator::RecordObject
    SkyboxManager::getRecordObject(game::Camera camera, f32 timeAlive) const
    {
        return game::FrameGenerator::RecordObject {
            .transform {camera.getTransform()},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {this->pipeline},
            .descriptors {this->game->getGlobalInfoDescriptorSet(), nullptr, nullptr, nullptr},
            .record_func {[=](vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, u32 id)
                          {
                              struct PushConstants
                              {
                                  glm::mat4 model_matrix;
                                  glm::vec4 camera_position;
                                  glm::vec4 camera_normal;
                                  u32       mvp_matricies_id;
                                  u32       draw_id;
                                  f32       time;
                              };

                              PushConstants pc {
                                  .model_matrix {camera.getModelMatrix()},
                                  .camera_position {glm::vec4 {camera.getPosition(), 0.0}},
                                  .camera_normal {glm::vec4 {camera.getForwardVector(), 0.0}},
                                  .mvp_matricies_id {id},
                                  .draw_id {0},
                                  .time {timeAlive}};

                              commandBuffer.pushConstants(
                                  layout,
                                  vk::ShaderStageFlagBits::eVertex
                                      | vk::ShaderStageFlagBits::eFragment,
                                  0,
                                  sizeof(PushConstants),
                                  &pc);

                              commandBuffer.draw(3, 1, 0, 0);
                          }}};
    }
} // namespace verdigris