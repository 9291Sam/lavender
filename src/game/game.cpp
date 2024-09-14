#include "game.hpp"
#include "ec_manager.hpp"
#include "frame_generator.hpp"
#include <gfx/renderer.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <shaders/shaders.hpp>
#include <util/log.hpp>


namespace game
{
    Game::Game()
        : renderer {std::make_unique<gfx::Renderer>()}
        , frame_generator {std::make_unique<FrameGenerator>(&*this->renderer)}
        , should_game_keep_ticking {true}
    {}

    Game::~Game() noexcept = default;

    void Game::run()
    {
        util::logTrace(
            "{} {} {}",
            game::BarComponent {}.getId(),
            game::FooComponent {}.getId(),
            game::FooComponent {}.getId());

        std::shared_ptr<vk::UniquePipeline> trianglePipeline =
            this->renderer->getAllocator()->cachePipeline(
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                    .stages {{
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eVertex},
                            .shader {this->renderer->getAllocator()
                                         ->cacheShaderModule(shaders::load(
                                             "build/src/shaders/"
                                             "triangle.vert.bin"))},
                            .entry_point {"main"},
                        },
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eFragment},
                            .shader {this->renderer->getAllocator()
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
                        this->renderer->getAllocator()->cachePipelineLayout(
                            gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                                .descriptors {},
                                .push_constants {std::nullopt},
                            })},
                });

        game::FrameGenerator::RecordObject triangleRecordObject {
            .render_pass {
                game::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {trianglePipeline},
            .descriptors {{nullptr, nullptr, nullptr, nullptr}},
            .record_func {[](vk::CommandBuffer commandBuffer)
                          {
                              commandBuffer.draw(3, 1, 0, 0);
                          }},
        };

        std::vector<game::FrameGenerator::RecordObject> draws {};
        draws.push_back(triangleRecordObject);

        while (!this->renderer->shouldWindowClose())
        {
            this->frame_generator->generateFrame(draws);
        }
    }
} // namespace game
