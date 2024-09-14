#include "gfx/renderer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "shaders/shaders.hpp"
#include "util/log.hpp"
#include <cstdlib>
#include <exception>
#include <fstream>
#include <game/frame/frame_generator.hpp>
#include <gfx/vulkan/allocator.hpp>
#include <gfx/vulkan/device.hpp>
#include <optional>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        const gfx::Renderer         renderer {};
        game::frame::FrameGenerator frameGenerator {&renderer};

        std::shared_ptr<vk::UniquePipeline> trianglePipeline =
            renderer.getAllocator()->cachePipeline(
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                    .stages {{
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eVertex},
                            .shader {renderer.getAllocator()->cacheShaderModule(
                                shaders::load(
                                    "build/src/shaders/triangle.vert.bin"))},
                            .entry_point {"main"},
                        },
                        gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                            .stage {vk::ShaderStageFlagBits::eFragment},
                            .shader {renderer.getAllocator()->cacheShaderModule(
                                shaders::load(
                                    "build/src/shaders/triangle.frag.bin"))},
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
                    .layout {renderer.getAllocator()->cachePipelineLayout(
                        gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                            .push_constants {std::nullopt}, .descriptors {}})},
                });

        game::frame::FrameGenerator::RecordObject triangleRecordObject {
            .render_pass {
                game::frame::FrameGenerator::DynamicRenderingPass::SimpleColor},
            .pipeline {trianglePipeline},
            .descriptors {{nullptr, nullptr, nullptr, nullptr}},
            .record_func {[](vk::CommandBuffer commandBuffer)
                          {
                              commandBuffer.draw(3, 1, 0, 0);
                          }},
        };

        std::vector<game::frame::FrameGenerator::RecordObject> draws {};
        draws.push_back(triangleRecordObject);

        while (!renderer.shouldWindowClose())
        {
            frameGenerator.generateFrame(draws);
        }
    }
    catch (const std::exception& e)
    {
        util::logFatal("Lavender has crashed! | {}", e.what());
    }
    catch (...)
    {
        util::logFatal("Lavender has crashed! | Non Exception Thrown!");
    }

    util::logLog("lavender exited successfully!");

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
