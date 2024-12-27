
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
#include <mutex>
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

        this->triangle.addComponent(TriangleComponent {.transform {
            .translation {glm::vec3 {1.323123, 32.3232123f, 3.8473f}},
            .scale {glm::vec3 {1.0f, 1.0f, 1.0f}}}});

        this->camera.addPosition({79.606, 42.586, -9.784});
        this->camera.addPitch(0.397f);
        this->camera.addYaw(5.17f);

        voxel::MaterialBrick brick {};

        // brick.write(voxel::BrickLocalPosition {{0, 0, 0}}, voxel::Voxel::Diamond);

        std::mt19937_64                     gen {73847375}; // NOLINT
        std::uniform_real_distribution<f32> dist {0.0f, 1.0f};
        std::uniform_real_distribution<f32> distN {-1.0f, 1.0f};

        for (int i = 0; i < 16; ++i)
        {
            brick.modifyOverVoxels(
                [&](auto bp, voxel::Voxel& v)
                {
                    if (bp.x < 6 && bp.y < 6 && bp.z < 6)
                    {
                        v = voxel::Voxel::Diamond;
                    }
                    // // {
                    // v = static_cast<voxel::Voxel>(dist(gen) * 17.99f);
                    // // }
                });

            this->cubes.push_back(
                {//     glm::vec3 {
                 //      distN(gen) * 64.0f,
                 //      (distN(gen) * 32.0f) + 32.0f,
                 //      distN(gen) * 64.0f,
                 //  },
                 glm::vec3 {
                     0.0f,
                     8.0f,
                     0.0f,
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

        // TODO: make not static lol
        static float verticalVelocity = 0.0f;    // Initial velocity for gravity
        const float  gravity          = -512.0f; // Gravitational acceleration (m/s²)
        const float  maxFallSpeed     = -512.0f; // Terminal velocity

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

        if (this->game->getRenderer()->getWindow()->isActionActive(
                gfx::Window::Action::PlayerMoveUp))
        {
            if (verticalVelocity == 0.0f)
            {
                verticalVelocity += 128.0f;
            }
        }

        newPosition.y = previousPosition.y;

        for (Flyer& f : this->fliers)
        {
            f.update(deltaTime);
        }

        glm::vec3 currentPosition = this->camera.getPosition();
        glm::vec3 displacement    = newPosition - currentPosition;

        if (this->time_alive > 5.0f)
        {
            verticalVelocity += gravity * deltaTime;
            verticalVelocity = std::max(verticalVelocity, maxFallSpeed);

            displacement.y += verticalVelocity * deltaTime;
        }

        // HACK: just prevent it from mattering lol
        // if (glm::length(displacement) > 1.0f)
        // {
        //     displacement = glm::normalize(displacement) * 0.999999f;
        // }

        glm::vec3 resolvedPosition = currentPosition;

        auto readVoxelOpacity = [&](voxel::WorldPosition p)
        {
            if (p.y < 64)
            {
                return true;
            }
            return this->voxel_world.readVoxelOccupied({&p, 1}).test(0);
        };

        glm::vec3 testPositionX = resolvedPosition + glm::vec3(displacement.x, 0.0f, 0.0f);
        if (!readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionX)}))
        {
            resolvedPosition.x += displacement.x;
        }

        glm::vec3 testPositionY = resolvedPosition + glm::vec3(0.0f, displacement.y, 0.0f);
        if (!readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionY)}))
        {
            resolvedPosition.y += displacement.y;
        }
        else
        {
            verticalVelocity = 0.0f;

            glm::vec3 upwardPosition = glm::floor(newPosition);
            bool      tooFar         = false;

            int steps = 0;
            while (!tooFar && readVoxelOpacity(voxel::WorldPosition {glm::floor(upwardPosition)}))
            {
                upwardPosition.y += 1.0f;

                if (steps++ > 3)
                {
                    tooFar = true;
                }
            }

            if (!tooFar)
            {
                resolvedPosition.y = upwardPosition.y;
            }
        }

        glm::vec3 testPositionZ = resolvedPosition + glm::vec3(0.0f, 0.0f, displacement.z);
        if (!readVoxelOpacity(voxel::WorldPosition {glm::floor(testPositionZ)}))
        {
            resolvedPosition.z += displacement.z;
        }

        this->camera.setPosition(resolvedPosition);

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

        auto realCamera = this->camera;
        realCamera.addPosition({0.0, 32.0f, 0.0});

        std::vector<game::FrameGenerator::RecordObject> draws {
            this->voxel_world.onFrameUpdate(realCamera, profilerTaskGenerator)};

        profilerTaskGenerator.stamp("World Update");

        this->time_alive += deltaTime;

        static std::once_flag once {};
        std::call_once(
            once,
            [&]()
            {
                float degoff = 0.0f;
                for (const auto& [pos, o] : this->cubes)
                {
                    const f32 x =
                        (64.0f
                         * std::sin(
                             degoff + this->time_alive * (1.0f + std::fmod<float>(pos.y, 1.3f))))
                        + pos.x;
                    const f32 z =
                        (64.0f
                         * std::cos(
                             degoff + this->time_alive * (1.0f + std::fmod<float>(pos.y, 1.3f))))
                        + pos.z;

                    degoff += 1.57079633 / 4;

                    this->voxel_world.setVoxelObjectPosition(
                        o, voxel::WorldPosition {{x, pos.y, z}});
                }
            });

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
            .main_scene_camera {realCamera},
            .record_objects {std::move(draws)},
            .generator {profilerTaskGenerator}};
    }

    void Verdigris::onTick(float) const {}

} // namespace verdigris
