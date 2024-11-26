#include "skybox_manager.hpp"
#include "game/game.hpp"
#include "gfx/renderer.hpp"
#include "util/static_filesystem.hpp"

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
                          .push_constants {},
                          .name {"Skybox Pipeline Layout"}})},
                  .name {"Skybox Pipeline"}})}
        , game {game_}
    {}

    game::FrameGenerator::RecordObject SkyboxManager::getRecordObject() const
    {
        return game::FrameGenerator::RecordObject {
            .transform {},
            .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {this->pipeline},
            .descriptors {this->game->getGlobalInfoDescriptorSet(), nullptr, nullptr, nullptr},
            .record_func {[](vk::CommandBuffer commandBuffer, vk::PipelineLayout, u32)
                          {
                              commandBuffer.draw(3, 1, 0, 0);
                          }}};
    }
} // namespace verdigris