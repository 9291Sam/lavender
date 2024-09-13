#include "gfx/renderer.hpp"
#include "gfx/vulkan/swapchain.hpp"
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

static std::vector<std::byte> readFileToByteVector(const std::string& filepath)
{
    // Open the file in binary mode and move the file pointer to the end
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);

    // Check if the file is open
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    // Get the size of the file
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create a vector to hold the file data
    std::vector<std::byte> buffer(fileSize);

    // Read the file into the buffer
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        throw std::runtime_error("Error reading file: " + filepath);
    }

    return buffer;
}

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        const gfx::Renderer         renderer {};
        game::frame::FrameGenerator frameGenerator {&renderer};

        auto vertex = readFileToByteVector("src/vertex.vert.bin");
        auto frag   = readFileToByteVector("src/frag.frag.bin");

        auto vertexModule = renderer.getAllocator()->cacheShaderModule(vertex);
        auto fragmentModule = renderer.getAllocator()->cacheShaderModule(frag);

        std::vector shaders = {
            gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                .stage {vk::ShaderStageFlagBits::eVertex},
                .shader {vertexModule},
                .entry_point {"main"},
            },
            gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                .stage {vk::ShaderStageFlagBits::eFragment},
                .shader {fragmentModule},
                .entry_point {"main"},
            },
        };

        std::shared_ptr<vk::UniquePipeline> trianglePipeline =
            renderer.getAllocator()->cachePipeline(
                gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                    .stages {shaders},
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
