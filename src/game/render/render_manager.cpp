#include "render_manager.hpp"
#include "game/camera.hpp"
#include "game/ec/components.hpp"
#include "game/ec/ec_manager.hpp"
#include "game/frame_generator.hpp"
#include "triangle_component.hpp"
#include "util/log.hpp"
#include <game/game.hpp>
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/fwd.hpp>
#include <shaders/shaders.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game::render
{
    RenderManager::RenderManager(const game::Game* game_)
        : game {game_}
        , camera {util::Mutex {Camera {glm::vec3 {}}}}
    {
        this->triangle_pipeline =
            this->game->getRenderer()->getAllocator()->cachePipeline(
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                    .stages {{
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eVertex},
                            .shader {this->game->getRenderer()
                                         ->getAllocator()
                                         ->cacheShaderModule(shaders::load(
                                             "build/src/shaders/"
                                             "triangle.vert.bin"))},
                            .entry_point {"main"},
                        },
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eFragment},
                            .shader {this->game->getRenderer()
                                         ->getAllocator()
                                         ->cacheShaderModule(shaders::load(
                                             "build/src/shaders/"
                                             "triangle.frag.bin"))},
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
                    .depth_test_enable {false},
                    .depth_write_enable {false},
                    .depth_compare_op {vk::CompareOp::eNever},
                    .color_format {gfx::Renderer::ColorFormat.format},
                    .depth_format {},
                    .layout {
                        this->game->getRenderer()
                            ->getAllocator()
                            ->cachePipelineLayout(
                                gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                                    .descriptors {},
                                    .push_constants {vk::PushConstantRange {
                                        .offset {0},
                                        .size {64},
                                        .stageFlags {
                                            vk::ShaderStageFlagBits::eVertex}}},
                                })},
                });
    }

    void RenderManager::setCamera(Camera c) const noexcept
    {
        this->camera.lock(
            [&](Camera& gameCamera)
            {
                gameCamera = c;
            });
    }

    std::vector<FrameGenerator::RecordObject>
    RenderManager::generateFrameObjects()
    {
        Camera camera = this->camera.copyInner();
        std::vector<game::FrameGenerator::RecordObject> draws {};

        this->game->getECManager()->iterateComponents<TriangleComponent>(
            [&](ec::Entity, const TriangleComponent& c)
            {
                draws.push_back(game::FrameGenerator::RecordObject {
                    .render_pass {game::FrameGenerator::DynamicRenderingPass::
                                      SimpleColor},
                    .pipeline {this->triangle_pipeline},
                    .descriptors {{nullptr, nullptr, nullptr, nullptr}},
                    .record_func {[=](vk::CommandBuffer commandBuffer)
                                  {
                                      const auto layout =
                                          this->game->getRenderer()
                                              ->getAllocator()
                                              ->lookupPipelineLayout(
                                                  **this->triangle_pipeline);

                                      const glm::mat4 mvpMatrix =
                                          camera.getPerspectiveMatrix(
                                              *this->game, c.transform);

                                      commandBuffer.pushConstants(
                                          **layout,
                                          vk::ShaderStageFlagBits::eVertex,
                                          0,
                                          sizeof(glm::mat4),
                                          &mvpMatrix);

                                      commandBuffer.draw(3, 1, 0, 0);
                                  }},
                });
            });

        return draws;
    }
} // namespace game::render