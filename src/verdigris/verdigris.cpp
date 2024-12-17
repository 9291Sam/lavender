
#include "verdigris.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "game/transform.hpp"
#include "gfx/profiler/task_generator.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/allocator.hpp"
#include "gfx/window.hpp"
#include "triangle_component.hpp"
#include "util/misc.hpp"
#include "util/static_filesystem.hpp"
#include "voxel/chunk_render_manager.hpp"
#include "voxel/structures.hpp"
#include "world/generator.hpp"
#include <FastNoise/FastNoise.h>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/range/numeric.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/random.hpp>
#include <random>
#include <utility>

namespace verdigris
{
    Verdigris::Verdigris(game::Game* game_) // NOLINT
        : game {game_}
        , triangle {ecs::createEntity()}
        , chunk_render_manager {this->game}
    {
        this->triangle_pipeline = this->game->getRenderer()->getAllocator()->cachePipeline(
            gfx::vulkan::CacheableGraphicsPipelineCreateInfo {
                .stages {{
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eVertex},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            staticFilesystem::loadShader("triangle.vert"),
                            "Triangle Vertex Shader")},
                        .entry_point {"main"},
                    },
                    gfx::vulkan::CacheablePipelineShaderStageCreateInfo {
                        .stage {vk::ShaderStageFlagBits::eFragment},
                        .shader {this->game->getRenderer()->getAllocator()->cacheShaderModule(
                            staticFilesystem::loadShader("triangle.frag"),
                            "Triangle Fragment Shader")},
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
                .depth_compare_op {vk::CompareOp::eLess},
                .color_format {gfx::Renderer::ColorFormat.format},
                .depth_format {gfx::Renderer::DepthFormat},
                .blend_enable {true},
                .layout {this->game->getRenderer()->getAllocator()->cachePipelineLayout(
                    gfx::vulkan::CacheablePipelineLayoutCreateInfo {
                        .descriptors {{this->game->getGlobalInfoDescriptorSetLayout()}},
                        .push_constants {vk::PushConstantRange {
                            .stageFlags {vk::ShaderStageFlagBits::eVertex},
                            .offset {0},
                            .size {64}}},
                        .name {"Triangle Pipeline Layout"}})},
                .name {"Triangle Pipeline"}});

        this->triangle.addComponent(TriangleComponent {
            .transform {.scale {glm::vec3 {25.0f, 25.0f, 25.0f}}} // namespace verdigris
        });

        // CpuMasterBuffer
        // meshoperation (copy of everything)
        // once its done upload it to the gpu

        this->camera.addPosition({79.606, 42.586, -9.784});
        this->camera.addPitch(0.397f);
        this->camera.addYaw(3.87f);

        // this->chunks.push_back(
        //     this->chunk_render_manager.createChunk(voxel::WorldPosition {{0, 0, 0}}));

        // std::vector<voxel::ChunkLocalUpdate> updates {};

        // // for (int i = 0; i < 64; ++i)
        // // {
        // //     for (int j = 0; j < 64; ++j)
        // //     {
        // //         updates.push_back(voxel::ChunkLocalUpdate {
        // //             voxel::ChunkLocalPosition {
        // //                 {i,
        // //                  static_cast<u8>(
        // //                      (8.0 * std::sin(i / 12.0)) + (8.0 * std::cos(j / 12.0)) + 17.0),
        // //                  j}},
        // //             static_cast<voxel::Voxel>((std::rand() % 11) + 1),
        // //             voxel::ChunkLocalUpdate::ShadowUpdate::ShadowCasting,
        // //             voxel::ChunkLocalUpdate::CameraVisibleUpdate::CameraVisible});
        // //     }
        // // }

        // this->chunk_render_manager.updateChunk(this->chunks.front(), updates);

        this->chunks = std::async(
            [this]
            {
                world::WorldChunkGenerator gen {789123};

                const std::int64_t radius = 4;

                std::vector<voxel::ChunkRenderManager::Chunk> threadChunks {};
                threadChunks.reserve(radius * 2 * radius * 3);

                for (int x = -radius; x <= radius; ++x)
                {
                    for (int z = -radius; z <= radius; ++z)
                    {
                        const voxel::WorldPosition pos {{64 * x, 0, 64 * z}};

                        threadChunks.push_back(this->chunk_render_manager.createChunk(pos));

                        std::vector<voxel::ChunkLocalUpdate> slowUpdates = gen.generateChunk(pos);

                        this->updates.lock(
                            [&](std::vector<std::pair<
                                    const voxel::ChunkRenderManager::Chunk*,
                                    std::vector<voxel::ChunkLocalUpdate>>>& lockedUpdates)
                            {
                                lockedUpdates.push_back({&*threadChunks.crbegin(), slowUpdates});
                            });
                    }
                }

                return threadChunks;
            });

        std::mt19937_64                     gen {73847375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};

        for (int i = 0; i < 1; ++i)
        {
            this->raytraced_lights.push_back(
                this->chunk_render_manager.createRaytracedLight(voxel::GpuRaytracedLight {
                    .position_and_max_distance {
                        glm::vec4 {// dist(gen) * 64,
                                   // dist(gen) * 64,
                                   // dist(gen) * 64,
                                   // dist(gen) * 64,

                                   32.0f,
                                   32.0f,
                                   32.0f,
                                   64.0f},
                    },
                    // .color_and_power {glm::vec4 {dist(gen), dist(gen), dist(gen), dist(gen) *
                    // 64}},
                    .color_and_power {1.0f, 1.0f, 1.0f, 32.0f},
                }));
        }
    }

    Verdigris::~Verdigris()
    {
        for (voxel::ChunkRenderManager::Chunk& c : this->chunks.get())
        {
            this->chunk_render_manager.destroyChunk(std::move(c));
        }

        for (voxel::ChunkRenderManager::RaytracedLight& l : this->raytraced_lights)
        {
            this->chunk_render_manager.destroyRaytracedLight(std::move(l));
        }
    }

    game::Game::GameState::OnFrameReturnData Verdigris::onFrame(float deltaTime) const
    {
        gfx::profiler::TaskGenerator profilerTaskGenerator {};

        this->updates.lock(
            [&](std::vector<std::pair<
                    const voxel::ChunkRenderManager::Chunk*,
                    std::vector<voxel::ChunkLocalUpdate>>>& lockedUpdates)
            {
                if (!lockedUpdates.empty())
                {
                    const auto it                       = lockedUpdates.crbegin();
                    const auto [chunkPtr, localUpdates] = *it;

                    this->chunk_render_manager.updateChunk(*chunkPtr, localUpdates);

                    lockedUpdates.pop_back();
                }
            });

        profilerTaskGenerator.stamp("Chunk Update");

        // TODO: moving diagonally is faster
        const float moveScale        = this->game->getRenderer()->getWindow()->isActionActive(
                                    gfx::Window::Action::PlayerSprint)
                                         ? 64.0f
                                         : 36.0f;
        const float rotateSpeedScale = 6.0f;

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::CloseWindow))
        {
            this->game->terminateGame();
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ToggleCursorAttachment))
        {
            this->game->getRenderer()->getWindow()->toggleCursor();
        }

        if (this->game->getRenderer()->getFrameNumber() == 4
            || this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::ResetPlayPosition))
        {
            this->camera.setPosition({79.606, 75.586, 16.78});
        }

        glm::vec3 previousPosition = this->camera.getPosition();

        glm::vec3 newPosition = previousPosition;

        // Adjust the new position based on input for movement directions
        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveForward))
        {
            newPosition += this->camera.getForwardVector() * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveBackward))
        {
            newPosition -= this->camera.getForwardVector() * deltaTime * moveScale;
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveLeft))
        {
            newPosition -= this->camera.getRightVector() * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveRight))
        {
            newPosition += this->camera.getRightVector() * deltaTime * moveScale;
        }

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveUp))
        {
            newPosition += game::Transform::UpVector * deltaTime * moveScale;
        }
        else if (this->game->getRenderer()->getWindow()->isActionActive(
                     gfx::Window::Action::PlayerMoveDown))
        {
            newPosition -= game::Transform::UpVector * deltaTime * moveScale;
        }

        this->camera.setPosition(newPosition);

        auto getMouseDeltaRadians = [&]
        {
            // each value from -1.0 -> 1.0 representing how much it moved
            // on the screen
            const auto [nDeltaX, nDeltaY] =
                this->game->getRenderer()->getWindow()->getScreenSpaceMouseDelta();

            const auto deltaRadiansX = (nDeltaX / 2) * this->game->getFovXRadians();
            const auto deltaRadiansY = (nDeltaY / 2) * this->game->getFovYRadians();

            return gfx::Window::Delta {.x {deltaRadiansX}, .y {deltaRadiansY}};
        };

        auto [xDelta, yDelta] = getMouseDeltaRadians();

        this->camera.addYaw(xDelta * rotateSpeedScale);
        this->camera.addPitch(yDelta * rotateSpeedScale);

        profilerTaskGenerator.stamp("Camera Update");

        std::vector<game::FrameGenerator::RecordObject> draws {};

        ecs::getGlobalECSManager()->iterateComponents<TriangleComponent>(
            [&](ecs::RawEntity, const TriangleComponent& c)
            {
                draws.push_back(game::FrameGenerator::RecordObject {
                    .transform {c.transform},
                    .render_pass {game::FrameGenerator::DynamicRenderingPass::SimpleColor},
                    .pipeline {this->triangle_pipeline},
                    .descriptors {
                        {this->game->getGlobalInfoDescriptorSet(), nullptr, nullptr, nullptr}},
                    .record_func {
                        [](vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, u32 id)
                        {
                            commandBuffer.pushConstants(
                                layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(u32), &id);

                            commandBuffer.draw(3, 1, 0, 0);
                        }},
                });
            });
        profilerTaskGenerator.stamp("triangle update");

        for (game::FrameGenerator::RecordObject& o :
             this->chunk_render_manager.processUpdatesAndGetDrawObjects(this->camera))
        {
            draws.push_back(std::move(o));
        }
        profilerTaskGenerator.stamp("chunk render manager update");

        return OnFrameReturnData {
            .main_scene_camera {this->camera},
            .record_objects {std::move(draws)},
            .generator {profilerTaskGenerator}};
    }

    void Verdigris::onTick(float) const {}

} // namespace verdigris
