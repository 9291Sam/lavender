
#include "verdigris.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "flyer.hpp"
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
#include "voxel/world_manager.hpp"
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
    Verdigris::Verdigris(game::Game* game_)
        : game {game_}
        , triangle {ecs::createEntity()}
        , voxel_world {this->game}
        , time_alive {0.0f}
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

        this->camera.addPosition({79.606, 42.586, -9.784});
        this->camera.addPitch(0.397f);
        this->camera.addYaw(5.17f);

        voxel::MaterialBrick brick {};

        std::mt19937_64                     gen {73847375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};
        std::uniform_real_distribution<f32> distN {-1.0f, 1.0f};

        for (int i = 0; i < 64; ++i)
        {
            brick.modifyOverVoxels(
                [&](auto bp, voxel::Voxel& v)
                {
                    if (bp.x < 4 && bp.y < 4 && bp.z < 4)
                    {
                        v = static_cast<voxel::Voxel>(dist(gen) * 17.99f);
                    }
                });

            this->cubes.push_back(
                {glm::vec3 {
                     distN(gen) * 64.0f,
                     (distN(gen) * 32.0f) + 32.0f,
                     distN(gen) * 64.0f,
                 },
                 this->voxel_world.createVoxelObject(
                     voxel::LinearVoxelVolume {brick}, voxel::WorldPosition {{0, 0, 0}})});
        }
    }

    Verdigris::~Verdigris()
    {
        for (auto& [pos, o] : this->cubes)
        {
            this->voxel_world.destroyVoxelObject(std::move(o));
        }
    }

    game::Game::GameState::OnFrameReturnData Verdigris::onFrame(float deltaTime) const
    {
        gfx::profiler::TaskGenerator profilerTaskGenerator {};

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

        if (this->game->getRenderer()->getWindow()->isActionActive(gfx::Window::Action::SpawnFlyer))
        {
            this->fliers.push_back(
                Flyer {camera.getPosition(), camera.getForwardVector(), 16.0f, &this->voxel_world});
        }

        for (Flyer& f : this->fliers)
        {
            f.update(deltaTime);
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

        std::vector<game::FrameGenerator::RecordObject> draws {
            this->voxel_world.onFrameUpdate(this->camera, profilerTaskGenerator)};

        profilerTaskGenerator.stamp("World Update");

        this->time_alive += deltaTime;

        for (const auto& [pos, o] : this->cubes)
        {
            const f32 x =
                (32.0f * std::sin(this->time_alive * (2.0f + std::fmod<float>(pos.y, 1.3f))))
                + pos.x;
            const f32 z =
                (32.0f * std::cos(this->time_alive * (2.0f + std::fmod<float>(pos.y, 1.3f))))
                + pos.z;

            this->voxel_world.setVoxelObjectPosition(o, voxel::WorldPosition {{x, pos.y, z}});
        }

        profilerTaskGenerator.stamp("update block");

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

        profilerTaskGenerator.stamp("Triangle Propagation");

        return OnFrameReturnData {
            .main_scene_camera {this->camera},
            .record_objects {std::move(draws)},
            .generator {profilerTaskGenerator}};
    }

    void Verdigris::onTick(float) const {}

} // namespace verdigris
